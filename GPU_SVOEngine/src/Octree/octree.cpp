#include "Octree/octree.hpp"

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

OctreeNode::OctreeNode()
{
	far.ptr = 0;
}

size_t Octree::getSize() const
{
	return data.size();
}

size_t Octree::getByteSize() const
{
	return data.size() * sizeof(OctreeNode);
}

OctreeNode& Octree::operator[](const size_t index)
{
	return data[index];
}

const OctreeNode& Octree::operator[](const size_t index) const
{
	return data[index];
}

bool populateRec(Octree& octree, const NearPtr parentPos, const uint8_t currentDepth, const uint8_t maxDepth)
{
	if (currentDepth >= maxDepth)
	{
		octree[parentPos.getPtr()].leaf.color = {static_cast<uint8_t>(rand() % 256), static_cast<uint8_t>(rand() % 256), static_cast<uint8_t>(rand() % 256)};
		return true;
	}

	const NearPtr childPtr = NearPtr(octree.getSize(), false);
	bool hasChildren = false;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (rand() % 2 == 0)
		{
			continue;
		}

		hasChildren = true;
		octree.addChild({});
		octree[parentPos.getPtr()].branch.childMask.setBit(i, true);
	}

	if (hasChildren)
		octree[parentPos.getPtr()].branch.ptr = childPtr;
	else
		return false;

	uint8_t count = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (!octree[parentPos.getPtr()].branch.childMask.getBit(i))
		{
			continue;
		}

		const NearPtr childPos = NearPtr(childPtr.getPtr() + count, false);
		if (populateRec(octree, childPos, currentDepth + 1, maxDepth))
		{
			octree[parentPos.getPtr()].branch.leafMask.setBit(i, true);
		}
		++count;
	}

	return false;
}

void Octree::populateSample(const uint8_t depth)
{
	addChild({});
	populateRec(*this, NearPtr(data.size() - 1, false), 0, depth);
}

void Octree::addChild(const OctreeNode child)
{
	data.push_back(child);
}

void* Octree::getData()
{
	return data.data();
}
