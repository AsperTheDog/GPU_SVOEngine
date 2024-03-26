#include "Octree/octree.hpp"

#include "logger.hpp"

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

glm::u8vec3 shiftColor(const glm::u8vec3 color)
{
    int r = std::min(std::max(color.r + (rand() % 2 == 0 ? 1 : -1), 0), 15);
    int g = std::min(std::max(color.g + (rand() % 2 == 0 ? 1 : -1), 0), 15);
    int b = std::min(std::max(color.b + (rand() % 2 == 0 ? 1 : -1), 0), 15);
    return {r, g, b};
}

bool populateRec(Octree& octree, const size_t parentPos, const uint8_t currentDepth, const uint8_t maxDepth, const uint8_t octant, glm::u8vec3 color)
{
	if (currentDepth >= maxDepth)
	{
        LeafNode leaf{0};
        //leaf.color = {static_cast<uint8_t>(rand() % 15), static_cast<uint8_t>(rand() % 15), static_cast<uint8_t>(rand() % 15)};
        leaf.color = color;
        leaf.normal = {0, 0, 0};
	    leaf.material = 0;
	    octree[parentPos] = leaf.toRaw();
        //Logger::print("Leaf at " + std::to_string(parentPos) + " with color " + std::to_string(leaf.color.r) + " " + std::to_string(leaf.color.g) + " " + std::to_string(leaf.color.b) + " for octant " + std::to_string(octant));
	    return true;
	}

    const size_t childIdx = octree.getSize() - parentPos;
	bool hasChildren = false;
    BranchNode parent = BranchNode(octree[parentPos]);
    for (uint8_t i = 0; i < 8; i++)
	{
		if ((static_cast<float>(rand()) / RAND_MAX) > 0.6)
		{
			continue;
		}

		hasChildren = true;
		octree.addChild(0);
		parent.childMask.setBit(i, true);
        //Logger::print("Adding child at " + std::to_string(octree.getSize() - 1) + " with parent " + std::to_string(parentPos));
	}

    
	if (hasChildren)
	{
        NearPtr childPtr{0, false};
        if (childIdx > 0x7FFF)
        {
            childPtr = octree.pushFarPtr(childIdx);
        }
        else
        {
            childPtr = NearPtr(static_cast<uint16_t>(childIdx), false);
        }
	    parent.ptr = childPtr;
	}
	else
	{
        //Logger::print("Parent " + std::to_string(parentPos) + " has no children");
	    return false;
	}

    //if (parent.ptr.isFar())
    //{
    //    Logger::print("Parent " + std::to_string(parentPos) + " is far and points to the farPtr " + std::to_string(parent.ptr.getPtr()) +  " (" + std::to_string(octree.getFarPtr(parent.ptr.getPtr())) + ")");
    //}
    //else
    //{
    //    Logger::print("Parent " + std::to_string(parentPos) + " is near and points to " + std::to_string(parent.ptr.getPtr()));
    //}
    
	uint8_t count = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (!parent.childMask.getBit(i))
		{
			continue;
		}

        color = shiftColor(color);
		if (populateRec(octree, parentPos + childIdx + count, currentDepth + 1, maxDepth, i, color))
		{
			parent.leafMask.setBit(i, true);
		}
		++count;
	}
    octree[parentPos] = parent.toRaw();

	return false;
}

void Octree::populateSample(const uint8_t maxDepth)
{
    Logger::pushContext("Octree population");
	addChild(0);
	populateRec(*this, 0, 0, maxDepth, 0, glm::u8vec3(7, 7, 7));
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
