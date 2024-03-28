#include "Octree/octree.hpp"

#include "camera.hpp"
#include "utils/logger.hpp"

glm::vec3 childPositions[] = {
    glm::vec3(0, 0, 0),
    glm::vec3(0, 0, 1),
    glm::vec3(0, 1, 0),
    glm::vec3(0, 1, 1),
    glm::vec3(1, 0, 0),
    glm::vec3(1, 0, 1),
    glm::vec3(1, 1, 0),
    glm::vec3(1, 1, 1)
};

NearPtr::NearPtr(const uint16_t ptr, const bool isFar)
	: raw(isFar ? ptr | 0x8000 : ptr & 0x7FFF)
{

}

uint16_t NearPtr::getPtr() const
{
	// Set most significant bit to 0
	return raw & 0x7FFF;
}

bool NearPtr::isFar() const
{
	// Check most significant bit
	return (raw & 0x8000) != 0;
}

uint16_t NearPtr::toRaw() const
{
    return raw;
}

BitField::BitField(const uint8_t field) : field(field)
{
}

bool BitField::getBit(const uint8_t index) const
{
	// Check if bit is set
	return (field & (1 << index)) != 0;
}

void BitField::setBit(const uint8_t index, const bool value)
{
	// Set or clear bit
	if (value)
	{
		field |= (1 << index);
	}
	else
	{
		field &= ~(1 << index);
	}
}

uint8_t BitField::toRaw() const
{
    return field;
}

BranchNode::BranchNode(const uint32_t raw)
{
    const bool farFlag = (raw & 0x80000000) >> 31;
    const uint16_t address = (raw & 0x7FFF0000) >> 16;
    ptr = NearPtr(address, farFlag);
    childMask = BitField((raw & 0x0000FF00) >> 8);
    leafMask = BitField(raw & 0x000000FF);
}

uint32_t BranchNode::toRaw() const
{
    return ptr.toRaw() << 16 | childMask.toRaw() << 8 | leafMask.toRaw();
}

LeafNode::LeafNode(const uint32_t raw)
{
    material = (raw & 0xFF000000) >> 24;
    normal.x = (raw & 0x00F00000) >> 20;
    normal.y = (raw & 0x000F0000) >> 16;
    normal.z = (raw & 0x0000F000) >> 12;
    color.x =  (raw & 0x00000F00) >> 8;
    color.y =  (raw & 0x000000F0) >> 4;
    color.z =   raw & 0x0000000F;
}

uint32_t LeafNode::toRaw() const
{
    const glm::u8vec3 tmpNormal = glm::min(normal, glm::u8vec3(15));
    const glm::u8vec3 tmpColor = glm::min(color, glm::u8vec3(15));
    return material << 24 | tmpNormal.x << 20 | tmpNormal.y << 16 | tmpNormal.z << 12 | tmpColor.x << 8 | tmpColor.y << 4 | tmpColor.z;
}

size_t Octree::getSize() const
{
	return data.size();
}

size_t Octree::getByteSize() const
{
	return data.size() * sizeof(uint32_t);
}

uint32_t& Octree::operator[](const size_t index)
{
	return data[index];
}

uint32_t Octree::getFarPtr(const size_t index) const
{
    return farPtrs[index];
}

uint8_t Octree::getDepth() const
{
    return depth;
}

void Octree::preallocate(const size_t size)
{
    data.reserve(size);
}

glm::vec3 shiftColor(const glm::vec3 color)
{
    float r = std::min(std::max(color.r + (rand() % 2 == 0 ? 0.4f : -0.4f), 0.0f), 15.0f);
    float g = std::min(std::max(color.g + (rand() % 2 == 0 ? 0.4f : -0.4f), 0.0f), 15.0f);
    float b = std::min(std::max(color.b + (rand() % 2 == 0 ? 0.4f : -0.4f), 0.0f), 15.0f);
    return {r, g, b};
}

bool Octree::populateRec(const size_t parentPos, const uint8_t currentDepth, const uint8_t maxDepth, glm::vec3 color)
{
    static uint64_t voxels = 0;
	if (currentDepth >= maxDepth)
	{
        LeafNode leaf{0};
        leaf.color = color;
        leaf.normal = {0, 0, 0};
	    leaf.material = 0;
	    get(parentPos) = leaf.toRaw();
        voxels++;
        return true;
	}

    const size_t childIdx = getSize() - parentPos;
	bool hasChildren = false;
    BranchNode parent = BranchNode(get(parentPos));
    for (uint8_t i = 0; i < 8; i++)
	{
		if ((static_cast<float>(rand()) / RAND_MAX) > 0.7)
		{
			continue;
		}

		hasChildren = true;
		addChild(0);
		parent.childMask.setBit(i, true);
	}

    
	if (hasChildren)
	{
        NearPtr childPtr{0, false};
        if (childIdx > 0x7FFF)
        {
            childPtr = pushFarPtr(childIdx);
        }
        else
        {
            childPtr = NearPtr(static_cast<uint16_t>(childIdx), false);
        }
	    parent.ptr = childPtr;
	}
	else
	{
	    return false;
	}
    
	uint8_t count = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (!parent.childMask.getBit(i))
		{
			continue;
		}

        color = shiftColor(color);
		if (populateRec(parentPos + childIdx + count, currentDepth + 1, maxDepth, color))
		{
			parent.leafMask.setBit(i, true);
		}
		++count;
	}
    get(parentPos) = parent.toRaw();

    if (currentDepth == 0)
    {
        Logger::print("Octree built", Logger::LevelBits::DEBUG);
        const float percentage = static_cast<float>(voxels) / static_cast<float>(pow(8, maxDepth - 1)) * 100.0f;
        Logger::print(" - Voxels: " + std::to_string(voxels) + " (" + std::to_string(static_cast<uint64_t>(percentage)) + "% dense)", Logger::LevelBits::DEBUG);
        Logger::print(" - Nodes: " + std::to_string(getSize()), Logger::LevelBits::DEBUG);
        Logger::print(" - Bytes: " + std::to_string(getByteSize()), Logger::LevelBits::DEBUG);
        Logger::print(" - Depth: " + std::to_string(maxDepth), Logger::LevelBits::DEBUG);
        Logger::print(" - Far pointers: " + std::to_string(farPtrs.size()), Logger::LevelBits::DEBUG);
    }

	return false;
}

void Octree::populateSample(const uint8_t maxDepth)
{
    Logger::pushContext("Octree population");
	addChild(0);
	populateRec(0, 0, maxDepth, glm::u8vec3(8, 3, 8));
    pack();
    Logger::popContext();
}

void Octree::pack()
{
    for (size_t i = farPtrs.size(); i != 0; i--)
    {
        addChild(farPtrs[i - 1]);
    }
}

void Octree::addChild(const uint32_t child)
{
	data.push_back(child);
}

void Octree::addChild(const BranchNode child)
{
    data.push_back(child.toRaw());
}

void Octree::addChild(const LeafNode child)
{
    data.push_back(child.toRaw());
}

void Octree::addChild(const FarNode child)
{
    data.push_back(child.ptr);
}

NearPtr Octree::pushFarPtr(const size_t childPos)
{
    farPtrs.push_back(static_cast<uint32_t>(childPos));
    return NearPtr(static_cast<uint16_t>(farPtrs.size() - 1), true);
}

void* Octree::getData()
{
	return data.data();
}

void Octree::clear()
{
    data.clear();
    farPtrs.clear();
    depth = 0;
}

uint32_t& Octree::get(const size_t index)
{
    return data[index];
}
