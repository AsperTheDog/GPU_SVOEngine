#include "VkBase/vulkan_memory.hpp"

#include <ranges>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include "logger.hpp"
#include "VkBase/vulkan_context.hpp"
#include "VkBase/vulkan_device.hpp"

std::string compactBytes(const VkDeviceSize bytes)
{
	const char* units[] = { "B", "KB", "MB", "GB", "TB" };
	uint32_t unit = 0;
	float exact = static_cast<float>(bytes);
	while (exact > 1024)
	{
		exact /= 1024;
		unit++;
	}
	return std::to_string(exact) + " " + units[unit];
}

std::string MemoryStructure::toString() const
{
	std::string str;
	for (uint32_t i = 0; i < m_memoryProperties.memoryHeapCount; i++)
	{
		str += "Memory Heap " + std::to_string(i) + ":\n";
		str += " - Size: " + compactBytes(m_memoryProperties.memoryHeaps[i].size) + "\n";
		str += " - Flags: " + string_VkMemoryHeapFlags(m_memoryProperties.memoryHeaps[i].flags) + "\n";
		str += " - Memory Types:\n";
		for (uint32_t j = 0; j < m_memoryProperties.memoryTypeCount; j++)
		{
			if (m_memoryProperties.memoryTypes[j].heapIndex == i)
			{
				str += "    - Memory Type " + std::to_string(j) + ": " + string_VkMemoryPropertyFlags(m_memoryProperties.memoryTypes[j].propertyFlags) + "\n";
			}
		}
	}
	return str;
}

std::optional<uint32_t> MemoryStructure::getStagingMemoryType(const uint32_t typeFilter) const
{
	std::vector<uint32_t> types = getMemoryTypes(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, typeFilter);

	if (types.empty())
		return std::nullopt;

	return types.front();
}

std::vector<uint32_t> MemoryStructure::getMemoryTypes(const VkMemoryPropertyFlags properties, const uint32_t typeFilter) const
{
	std::vector<uint32_t> suitableTypes{};
	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && doesMemoryContainProperties(i, properties)) {
            suitableTypes.push_back(i);
        }
	}
	return suitableTypes;
}

bool MemoryStructure::doesMemoryContainProperties(const uint32_t type, const VkMemoryPropertyFlags property) const
{
	return (m_memoryProperties.memoryTypes[type].propertyFlags & property) == property;
}

MemoryStructure::MemoryStructure(const VulkanGPU gpu)
{
	vkGetPhysicalDeviceMemoryProperties(gpu.m_vkHandle, &m_memoryProperties);
}

VkDeviceSize MemoryChunk::getSize() const
{
	return m_size;
}

uint32_t MemoryChunk::getMemoryType() const
{
	return m_memoryType;
}

bool MemoryChunk::isEmpty() const
{
	VkDeviceSize freeSize = 0;
	for (const auto& size : m_unallocatedData | std::views::values)
	{
		freeSize += size;
	}
	return freeSize == m_size;
}

MemoryChunk::MemoryBlock MemoryChunk::allocate(const VkDeviceSize newSize, const VkDeviceSize alignment)
{
	VkDeviceSize best = m_size;
	VkDeviceSize bestAlignOffset = 0;
	if (newSize > m_unallocatedData[m_biggestChunk])
		return {0, 0, m_id};

	for (const auto& [offset, size] : m_unallocatedData)
	{
		if (size < newSize) continue;

		const VkDeviceSize alignedOffset = offset > 0 ? offset + (alignment - (offset % alignment)) : 0;
		const VkDeviceSize offsetDifference = alignedOffset - offset;

		if (size + offsetDifference < newSize) continue;

		if (best == m_size || m_unallocatedData[best] > size)
		{
			best = offset;
			bestAlignOffset = offsetDifference;
		}
	}

	if (best == m_size)
		return {0, 0, m_id};

	const VkDeviceSize bestSize = m_unallocatedData[best];
	m_unallocatedData.erase(best);
	if (bestAlignOffset != 0)
	{
		m_unallocatedData[best] = bestAlignOffset;
		best += bestAlignOffset;
	}
	if (bestSize - bestAlignOffset != newSize)
	{
		m_unallocatedData[best + newSize] = (bestSize - bestAlignOffset) - newSize;
	}

	Logger::print("Allocated block of size " + std::to_string(newSize) + " at offset " + std::to_string(best) + " of memory type " + std::to_string(m_memoryType));

	if (m_biggestChunk == best)
	{
		for (const auto& [offset, size] : m_unallocatedData)
		{
			if (!m_unallocatedData.contains(m_biggestChunk) || size > m_unallocatedData[m_biggestChunk])
				m_biggestChunk = offset;
		}
	}

	m_unallocatedSize -= newSize;

	return {newSize, best, m_id};
}

void MemoryChunk::deallocate(const MemoryBlock& block)
{
	if (block.chunk != m_id)
		throw std::runtime_error("Block does not belong to this chunk!");

	m_unallocatedData[block.offset] = block.size;
	Logger::print("Deallocated block of size " + std::to_string(block.size) + " at offset " + std::to_string(block.offset) + " of memory type " + std::to_string(m_memoryType));

	m_unallocatedSize += block.size;

	defragment();
}

VkDeviceMemory MemoryChunk::operator*() const
{
	return m_memory;
}

VkDeviceSize MemoryChunk::getBiggestChunkSize() const
{
	return m_unallocatedData.empty() ? 0 : m_unallocatedData.at(m_biggestChunk);
}

VkDeviceSize MemoryChunk::getRemainingSize() const
{
	return m_unallocatedSize;
}

MemoryChunk::MemoryChunk(const VkDeviceSize size, const uint32_t memoryType, const VkDeviceMemory vkHandle)
	: m_size(size), m_memoryType(memoryType), m_memory(vkHandle), m_unallocatedSize(size)
{
	m_unallocatedData[0] = size;
}

void MemoryChunk::defragment()
{
	if (m_unallocatedSize == m_size)
	{
		Logger::print("No need to defragment empty memory chunk " + std::to_string(m_id));
		return;
	}
	Logger::pushContext("Memory defragmentation");
	Logger::print("Defragmenting memory chunk " + std::to_string(m_id));
	uint32_t mergeCount = 0;
	for (auto it = m_unallocatedData.begin(); it != m_unallocatedData.end();)
	{
		auto next = std::next(it);
		if (next == m_unallocatedData.end())
		{
			break;
		}
		if (next != m_unallocatedData.end() && it->first + it->second == next->first)
		{
			
			it->second += next->second;
			if (next->first == m_biggestChunk || it->second > m_unallocatedData[m_biggestChunk])
				m_biggestChunk = it->first;

			Logger::print("Merged blocks at offsets " + std::to_string(it->first) + " and " + std::to_string(next->first) + ", new size: " + std::to_string(it->second));
			mergeCount++;

			m_unallocatedData.erase(next);

		}
		else
		{
			++it;
		}
	}
	Logger::print("Defragmented " + std::to_string(mergeCount) + " blocks");
	Logger::popContext();
}

VulkanMemoryAllocator::VulkanMemoryAllocator(const VulkanDevice& device, const VkDeviceSize defaultChunkSize)
	: m_memoryStructure(device.getGPU()), m_chunkSize(defaultChunkSize), m_device(device.getID())
{

}

void VulkanMemoryAllocator::free()
{
	for (const MemoryChunk& memoryBlock : m_memoryChunks)
	{
		vkFreeMemory(VulkanContext::getDevice(m_device).m_vkHandle, memoryBlock.m_memory, nullptr);
	}
	m_memoryChunks.clear();
}

MemoryChunk::MemoryBlock VulkanMemoryAllocator::allocate(const VkDeviceSize size, const VkDeviceSize alignment, const uint32_t memoryType)
{
	VkDeviceSize chunkSize = m_chunkSize;
	if (size < m_chunkSize)
	{
		for (auto& memoryChunk : m_memoryChunks)
		{
			if (memoryChunk.m_memoryType == memoryType)
			{
				const MemoryChunk::MemoryBlock block = memoryChunk.allocate(size, alignment);
				if (block.size != 0) return block;
			}
		}
	}
	else
	{
		chunkSize = size;
	}
	

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = chunkSize;
	allocInfo.memoryTypeIndex = memoryType;

	VkDeviceMemory memory;
	if (vkAllocateMemory(VulkanContext::getDevice(m_device).m_vkHandle, &allocInfo, nullptr, &memory) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory");
	}

	m_memoryChunks.push_back(MemoryChunk(chunkSize, memoryType, memory));
	Logger::print("Allocated chunk of size " + compactBytes(chunkSize) + " of memory type " + std::to_string(memoryType) + " (ID: " + std::to_string(m_memoryChunks.back().getID()) + ")");
	return m_memoryChunks.back().allocate(size, alignment);
}

MemoryChunk::MemoryBlock VulkanMemoryAllocator::searchAndAllocate(const VkDeviceSize size, const VkDeviceSize alignment, const MemoryPropertyPreferences properties, const uint32_t typeFilter, const bool includeHidden)
{
	const std::vector<uint32_t> memoryType = m_memoryStructure.getMemoryTypes(properties.desiredProperties, typeFilter);
	uint32_t bestType = 0;
	VkDeviceSize bestSize = 0;
	bool doesBestHaveUndesired = false;
	for (const auto& type : memoryType)
	{
		if (!includeHidden && m_hiddenTypes.contains(type))
			continue;

		const bool doesMemoryHaveUndesired = m_memoryStructure.doesMemoryContainProperties(type, properties.undesiredProperties);
		if (!properties.allowUndesired && doesMemoryHaveUndesired)
			continue;

		if (bestSize != 0 && !doesBestHaveUndesired && doesMemoryHaveUndesired)
			continue;

		if (suitableChunkExists(type, size))
			return allocate(size, alignment, type);

		const VkDeviceSize remainingSize = getRemainingSize(m_memoryStructure.m_memoryProperties.memoryTypes[type].heapIndex);
		if (remainingSize >= bestSize)
		{
			bestType = type;
			bestSize = remainingSize;
			doesBestHaveUndesired = doesMemoryHaveUndesired;
		}
	}
	return allocate(size, alignment, bestType);
}

void VulkanMemoryAllocator::deallocate(const MemoryChunk::MemoryBlock& block)
{
	uint32_t chunkIndex = static_cast<uint32_t>(m_memoryChunks.size());
	for (uint32_t i = 0; i < m_memoryChunks.size(); i++)
	{
		if (m_memoryChunks[i].getID() == block.chunk)
		{
			chunkIndex = i;
			break;
		}
	}

	if (chunkIndex == m_memoryChunks.size())
	{
		throw std::runtime_error("Block does not belong to any chunk!");
	}

	m_memoryChunks[chunkIndex].deallocate(block);
	if (m_memoryChunks[chunkIndex].isEmpty())
	{
		vkFreeMemory(VulkanContext::getDevice(m_device).m_vkHandle, m_memoryChunks[chunkIndex].m_memory, nullptr);
		m_memoryChunks.erase(m_memoryChunks.begin() + chunkIndex);
		Logger::print("Freed empty chunk " + std::to_string(block.chunk));
	}
}

void VulkanMemoryAllocator::hideMemoryType(const uint32_t type)
{
	Logger::print("Hiding memory type " + std::to_string(type));
	m_hiddenTypes.insert(type);
}

void VulkanMemoryAllocator::unhideMemoryType(const uint32_t type)
{
	Logger::print("Unhiding memory type " + std::to_string(type));
	m_hiddenTypes.erase(type);
}

const MemoryStructure& VulkanMemoryAllocator::getMemoryStructure() const
{
	return m_memoryStructure;
}

VkDeviceSize VulkanMemoryAllocator::getRemainingSize(const uint32_t heap) const
{
	VkDeviceSize remainingSize = m_memoryStructure.m_memoryProperties.memoryHeaps[heap].size;
	for (const auto& chunk : m_memoryChunks)
	{
		if (m_memoryStructure.m_memoryProperties.memoryTypes[chunk.m_memoryType].heapIndex == heap)
		{
			remainingSize -= chunk.getSize();
		}
	}
	return remainingSize;
}

bool VulkanMemoryAllocator::suitableChunkExists(const uint32_t memoryType, const VkDeviceSize size) const
{
	for (const auto& chunk : m_memoryChunks)
	{
		if (chunk.m_memoryType == memoryType && chunk.getBiggestChunkSize() >= size)
		{
			return true;
		}
	}
	return false;
}

bool VulkanMemoryAllocator::isMemoryTypeHidden(const unsigned value) const
{
	return m_hiddenTypes.contains(value);
}

uint32_t VulkanMemoryAllocator::getChunkMemoryType(const uint32_t chunk) const
{
	for (const auto& memoryChunk : m_memoryChunks)
	{
		if (memoryChunk.getID() == chunk)
		{
			return memoryChunk.getMemoryType();
		}
	}

	throw std::runtime_error("Chunk not found");
}
