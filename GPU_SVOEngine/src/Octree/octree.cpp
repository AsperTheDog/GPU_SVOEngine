#include "octree.hpp"

#include <array>
#include <bitset>
#include <chrono>
#include <fstream>

#include <glm/gtx/string_cast.hpp>

#include "utils/logger.hpp"


glm::vec3 childPositions[] = {
    glm::vec3(-1, -1, -1),
    glm::vec3(-1, -1,  1),
    glm::vec3(-1,  1, -1),
    glm::vec3(-1,  1,  1),
    glm::vec3( 1, -1, -1),
    glm::vec3( 1, -1,  1),
    glm::vec3( 1,  1, -1),
    glm::vec3( 1,  1,  1)
};

NearPtr::NearPtr(const uint16_t ptr, const bool isFar)
    : addr(ptr & 0x7FFF), farFlag(isFar ? 1 : 0)
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
    return isFar() ? addr | 0x8000 : addr & 0x7FFF;
}

void NearPtr::setPtr(const uint16_t ptr)
{
    addr = ptr & 0x7FFF;
}

void NearPtr::setFar(const bool isFar)
{
    farFlag = isFar ? 1 : 0;
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

bool BitField::operator==(const BitField& other) const
{
    return field == other.field;
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

std::string DebugOctreeNode::toString(uint32_t position, const Type type) const
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

Octree::Octree(const uint8_t maxDepth)
    : depth(maxDepth)
{

}

Octree::Octree(const uint8_t maxDepth, const std::string_view outputFile)
    : depth(maxDepth), dumpFile(outputFile)
{

}

uint32_t Octree::getRaw(uint32_t index) const
{
#ifdef DEBUG_STRUCTURE
    return data[index].pack(types[index]);
#else
    return data[index];
#endif
}

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

bool Octree::isReversed() const
{
    return reversed;
}

void Octree::preallocate(const size_t size)
{
    data.reserve(size);
}

void Octree::generate(const ProcessFunc func, void* processData)
{
    data.clear();
    reversed = true;
    process = func;
    stats = OctreeStats{};
    const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    populate({ glm::vec3{0}, glm::vec3(static_cast<float>(1 << depth)) }, processData);
    const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    stats.constructionTime = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.f;
    Logger::print("Octree generated in " + std::to_string(stats.constructionTime) + "s", Logger::INFO);

    // Dump to file if specified
    if (!dumpFile.empty())
    {
        dump(dumpFile);
    }
}

NodeRef Octree::populateRec(AABB nodeShape, const uint8_t currentDepth, void* processData)
{
    NodeRef ref = process(nodeShape, currentDepth, depth, processData);
    if (!ref.exists || ref.isLeaf)
        return ref;

    BranchNode node{0};

    // Recurse into children
    std::array<NodeRef, 8> children;
    for (int8_t i = 7; i >= 0; i--)
    {
        nodeShape.halfSize *= 0.5f;
        nodeShape.center += childPositions[i] * nodeShape.halfSize;
        children[i] = populateRec(nodeShape, currentDepth + 1, processData);

        node.childMask.setBit(i, children[i].exists);
        node.leafMask.setBit(i, children[i].isLeaf);
    }
    if (node.childMask.toRaw() == 0)
    {
        ref.exists = false;
        return ref;
    }

    // Prepare addresses and far pointers
    BitField farMaskOld{0};
    BitField farMask{0};
    uint8_t farCount = 0;
    std::array<uint32_t, 8> addresses{};
    do
    {
        uint8_t validChildCount = 0;
        farMaskOld = farMask;
        for (int8_t i = 7; i >= 0; i--)
        {
            if (!children[i].exists) continue;
            children[i].pos = getSize() + validChildCount + farCount;
            validChildCount++;
            if (children[i].isLeaf) continue;
            addresses[i] = children[i].pos - children[i].childPos;
            if (addresses[i] > NEAR_PTR_MAX && farMask.getBit(i) == false)
            {
                farMask.setBit(i, true);
                farCount++;
            }
        }
    } while (farMask != farMaskOld);

    //Push far pointers to octree
    for (int8_t i = 7; i >= 0; i--)
    {
        if (!farMask.getBit(i)) continue;
        const FarNode ptr = FarNode(getSize() - children[i].childPos);
        addresses[i] = children[i].pos - getSize();
        addNode(ptr);
    }

    //Push children to octree
    for (int8_t i = 7; i >= 0; i--)
    {
        if (!children[i].exists) continue;
        if (children[i].isLeaf)
        {
            addNode(LeafNode(children[i].data));
        }
        else
        {
            BranchNode child = BranchNode(children[i].data);
            child.ptr = NearPtr(static_cast<uint16_t>(addresses[i]), farMask.getBit(i));
            addNode(child);
        }
    }

    // Resolve position and return
    uint8_t firstChild = 9;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (children[i].exists)
        {
            firstChild = std::min(firstChild, i);
            break;
        }
    }
    ref.childPos = children[firstChild].pos;
    ref.data = node.toRaw();
    return ref;
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
    if (!reversed)
        file.write(reinterpret_cast<const char*>(data.data()), getByteSize());
    else
    {
        for (uint32_t i = 1; i <= data.size(); i++)
        {
            const uint32_t raw = data[getSize() - i];
            file.write(reinterpret_cast<const char*>(&raw), sizeof(uint32_t));
        }
    }
    file.close();
}

void Octree::clear()
{
    data.clear();
    farPtrs.clear();
    depth = 0;
}

void Octree::populate(AABB nodeShape, void* processData)
{
    const NodeRef ref = populateRec({ glm::vec3{0}, glm::vec3(static_cast<float>(1 << depth)) }, 0, processData);
    if (!ref.exists)
    {
        Logger::print("Octree generation returned non-existent root. Resulting octree is empty or broken", Logger::WARN);
        return;
    }
    if (ref.isLeaf)
    {
        addNode(LeafNode(ref.data));
    }
    else
    {
        BranchNode node{ref.data};
        uint32_t childPtr = getSize() - ref.childPos;
        if (getSize() - ref.childPos > NEAR_PTR_MAX)
        {
            addNode(FarNode(childPtr));
            childPtr = ref.pos - getSize();
        }
        node.ptr = NearPtr(static_cast<uint16_t>(childPtr), false);
        addNode(node);
    }
}

#ifdef DEBUG_STRUCTURE
std::string Octree::toString() const
{
    std::stringstream ss{};
    ss << "Octree structure:\n";
    for (uint32_t i = 1; i <= data.size(); i++)
    {
        ss << data[getSize() - i].toString(i - 1, types[getSize() - i]) << '\n';
    }
    return ss.str();
}

Type Octree::getType(const uint32_t index) const
{
    return types[index];
}

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
