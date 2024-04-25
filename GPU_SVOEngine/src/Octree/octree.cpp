#include "octree.hpp"

#include <bitset>
#include <fstream>

#include <glm/gtx/string_cast.hpp>

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
    : farFlag(isFar ? 1 : 0), addr(ptr & NEAR_PTR_MAX)
{

}

uint16_t NearPtr::getPtr() const
{
    return addr;
}

bool NearPtr::isFar() const
{
    return farFlag == 1;
}

uint16_t NearPtr::toRaw() const
{
    return isFar() ? addr | 0x8000 : addr & NEAR_PTR_MAX;
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
    normalx = (raw & 0x00F00000) >> 20;
    normaly = (raw & 0x000F0000) >> 16;
    normalz = (raw & 0x0000F000) >> 12;
    colorx = (raw & 0x00000F00) >> 8;
    colory = (raw & 0x000000F0) >> 4;
    colorz = raw & 0x0000000F;
}

void LeafNode::setNormal(const glm::u8vec3& normal)
{
    constexpr uint8_t max = 0xFF;
    normalx = std::min(normal.x, max);
    normaly = std::min(normal.y, max);
    normalz = std::min(normal.z, max);
}

void LeafNode::setColor(const glm::u8vec3& color)
{
    constexpr uint8_t max = 0xFF;
    colorx = std::min(color.x, max);
    colory = std::min(color.y, max);
    colorz = std::min(color.z, max);
}

glm::u8vec3 LeafNode::getNormal() const
{
    return { normalx, normaly, normalz };
}

glm::u8vec3 LeafNode::getColor() const
{
    return { colorx, colory, colorz };
}

uint32_t LeafNode::toRaw() const
{
    const glm::u8vec3 tmpNormal = glm::min(getNormal(), glm::u8vec3(15));
    const glm::u8vec3 tmpColor = glm::min(getColor(), glm::u8vec3(15));
    return material << 24 | tmpNormal.x << 20 | tmpNormal.y << 16 | tmpNormal.z << 12 | tmpColor.x << 8 | tmpColor.y << 4 | tmpColor.z;
}

FarNode::FarNode(const uint32_t raw)
{
    ptr = raw;
}

uint32_t FarNode::toRaw() const
{
    return ptr;
}

#ifdef DEBUG_STRUCTURE
DebugOctreeNode::Data::Data()
{
    memset(this, 0, sizeof(Data));
}

DebugOctreeNode::DebugOctreeNode(const Type type)
{
    switch (type)
    {
    case Type::BRANCH_NODE:
        data.branchNode = BranchNode(0);
        break;
    case Type::LEAF_NODE:
        data.leafNode = LeafNode(0);
        break;
    case Type::FAR_NODE:
        data.farNode = FarNode(0);
        break;
    }
}

void DebugOctreeNode::update(const uint32_t raw, const Type type)
{
    switch (type)
    {
    case Type::BRANCH_NODE:
        data.branchNode = BranchNode(raw);
        break;
    case Type::LEAF_NODE:
        data.leafNode = LeafNode(raw);
        break;
    case Type::FAR_NODE:
        data.farNode = FarNode(raw);
        break;
    }
}

uint32_t DebugOctreeNode::pack(const Type type) const
{
    switch (type)
    {
    case Type::BRANCH_NODE:
        return data.branchNode.toRaw();
    case Type::LEAF_NODE:
        return data.leafNode.toRaw();
    case Type::FAR_NODE:
        return data.farNode.toRaw();
    }
    return 0;
}

std::string DebugOctreeNode::toString(const uint32_t position, const Type type) const
{
    std::stringstream ss{};
    ss << "(" << position << ") ";
    switch (type)
    {
    case BRANCH_NODE:
        ss << "BranchNode: {";
        ss << "ptr: " << data.branchNode.ptr.getPtr() << " (absolute: " + std::to_string(data.branchNode.ptr.getPtr() + position) + "), ";
        ss << "isFar: " << data.branchNode.ptr.isFar() << ", ";
        ss << "leafMask: " << std::bitset<8>(data.branchNode.leafMask.toRaw()) << ", ";
        ss << "childMask: " << std::bitset<8>(data.branchNode.childMask.toRaw()) << " }";
        break;
    case LEAF_NODE:
        ss << "LeafNode: {";
        ss << "material: " << static_cast<uint32_t>(data.leafNode.material) << ", ";
        ss << "normal: " << glm::to_string(data.leafNode.getNormal()) << ", ";
        ss << "color: " << glm::to_string(data.leafNode.getColor()) << " }";
        break;
    case FAR_NODE:
        ss << "FarNode: {";
        ss << "target: " << data.farNode.toRaw() << " (absolute: " + std::to_string(data.farNode.toRaw() + position) + ") }";
        break;
    }
    return ss.str();
}
#endif

uint32_t Octree::getSize() const
{
    return static_cast<uint32_t>(data.size());
}

uint32_t Octree::getByteSize() const
{
    return static_cast<uint32_t>(data.size()) * sizeof(uint32_t);
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
    return { r, g, b };
}

bool Octree::populateRec(const uint32_t parentPos, const uint8_t currentDepth, const uint8_t maxDepth, glm::vec3 color, const float density)
{
    static uint64_t voxels = 0;
    if (currentDepth >= maxDepth)
    {
        LeafNode leaf{ 0 };
        leaf.setColor(color);
        leaf.setNormal({ 0, 0, 0 });
        leaf.material = 0;
        updateNode(parentPos, leaf);
        voxels++;
        return true;
    }

    const uint32_t childIdx = getSize() - parentPos;
    bool hasChildren = false;
    BranchNode parent = BranchNode(get(parentPos));
    parent.leafMask = currentDepth + 1 >= maxDepth ? BitField(0xFF) : BitField(0x00);

    uint8_t childCount = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (static_cast<float>(rand()) / RAND_MAX > density)
        {
            continue;
        }

        hasChildren = true;
        if (parent.leafMask.getBit(i))
            addNode(LeafNode(0));
        else
            addNode(BranchNode(0));
        parent.childMask.setBit(i, true);
        childCount++;
    }
    
    parent.leafMask = currentDepth + 1 >= maxDepth ? parent.childMask : BitField(0x00);

    if (hasChildren)
    {
        NearPtr childPtr{ 0, false };
        if (childIdx > NEAR_PTR_MAX)
            childPtr = pushFarPtr(parentPos, childIdx, parentPos + childCount);
        else
            childPtr = NearPtr(static_cast<uint16_t>(childIdx), false);
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
        populateRec(parentPos + childIdx + count, currentDepth + 1, maxDepth, color, density);
        ++count;
    }

    updateNode(parentPos, parent);

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

void Octree::generateRandomly(const uint8_t maxDepth, const float density)
{
    Logger::pushContext("Octree population");
    addNode(BranchNode(0));
    populateRec(0, 0, maxDepth, glm::u8vec3(8, 3, 8), density);
    pack();
    Logger::popContext();
}

void Octree::pack()
{
    for (size_t i = farPtrs.size(); i != 0; i--)
    {
        addNode(FarNode(farPtrs[i - 1].destinationPos));
    }
}

void Octree::addNode(const BranchNode child)
{
#ifdef DEBUG_STRUCTURE
    DebugOctreeNode node(Type::BRANCH_NODE);
    node.data.branchNode = child;
    data.push_back(node);
    types.push_back(Type::BRANCH_NODE);
#else
    data.push_back(child.toRaw());
#endif
}

void Octree::addNode(const LeafNode child)
{
#ifdef DEBUG_STRUCTURE
    DebugOctreeNode node(Type::LEAF_NODE);
    node.data.leafNode = child;
    data.push_back(node);
    types.push_back(Type::LEAF_NODE);
#else
    data.push_back(child.toRaw());
#endif
}

void Octree::addNode(const FarNode child)
{
#ifdef DEBUG_STRUCTURE
    DebugOctreeNode node(Type::FAR_NODE);
    node.data.farNode = child;
    data.push_back(node);
    types.push_back(Type::FAR_NODE);
#else
    data.push_back(child.toRaw());
#endif
}

void Octree::updateNode(uint32_t index, BranchNode node)
{
#ifdef DEBUG_STRUCTURE
    data[index].update(node.toRaw(), Type::BRANCH_NODE);
#else
    data[index] = node.toRaw();
#endif
}

void Octree::updateNode(uint32_t index, LeafNode node)
{
#ifdef DEBUG_STRUCTURE
    data[index].update(node.toRaw(), Type::LEAF_NODE);
#else
    data[index] = node.toRaw();
#endif
}

void Octree::updateNode(uint32_t index, FarNode node)
{
#ifdef DEBUG_STRUCTURE
    data[index].update(node.toRaw(), Type::FAR_NODE);
#else
    data[index] = node.toRaw();
#endif
}

NearPtr Octree::pushFarPtr(const uint32_t sourcePos, const uint32_t destinationPos, const uint32_t farNodePos)
{
    farPtrs.push_back({ sourcePos, destinationPos, farNodePos });
    return NearPtr(static_cast<uint16_t>(farPtrs.size() - 1), true);
}

void* Octree::getData()
{
    return data.data();
}

void Octree::dump(const std::string& filename) const
{
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), getByteSize());
    file.close();
}

void Octree::clear()
{
    data.clear();
    farPtrs.clear();
    depth = 0;
}

#ifdef DEBUG_STRUCTURE
uint32_t Octree::get(const uint32_t index) const
{
    const Type type = types[index];
    switch (type)
    {
    case Type::BRANCH_NODE:
        return data[index].data.branchNode.toRaw();
    case Type::LEAF_NODE:
        return data[index].data.leafNode.toRaw();
    case Type::FAR_NODE:
        return data[index].data.farNode.toRaw();
    }
    return 0;
}
#else
uint32_t& Octree::get(const uint32_t index)
{
    return data[index];
}
#endif
