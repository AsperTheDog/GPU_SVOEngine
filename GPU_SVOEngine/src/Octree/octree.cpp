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
    const bool farFlag =     (raw & 0x80000000) >> 31;
    const uint16_t address = (raw & 0x7FFF0000) >> 16;
    childMask =     BitField((raw & 0x0000FF00) >> 8);
    leafMask =       BitField(raw & 0x000000FF);
    ptr = NearPtr(address, farFlag);
}

uint32_t BranchNode::toRaw() const
{
    return ptr.toRaw() << 16 | childMask.toRaw() << 8 | leafMask.toRaw();
}

LeafNode::LeafNode(const uint64_t raw)
{
    uvx =      (raw & 0xFFF0000000000000) >> 52;
    uvy =      (raw & 0x000FFF0000000000) >> 40;
    material = (raw & 0x000000FFC0000000) >> 30;
    normalx =  (raw & 0x000000003FF00000) >> 20;
    normaly =  (raw & 0x00000000000FFC00) >> 10;
    normalz =   raw & 0x00000000000003FF;
}

void LeafNode::setUV(glm::vec2 uv)
{
    while (uv.x < 0.f) uv.x += 1.f;
    while (uv.y < 0.f) uv.y += 1.f;
    while (uv.x > 1.f) uv.x -= 1.f;
    while (uv.y > 1.f) uv.y -= 1.f;

    constexpr uint16_t max = 0xFFF;
    uvx = std::min(static_cast<uint16_t>(uv.x * max), max);
    uvy = std::min(static_cast<uint16_t>(uv.y * max), max);
}

void LeafNode::setNormal(const glm::vec3& normal)
{
    constexpr float max = 0x3FF;
    constexpr uint16_t half_max = 0x200;
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normaly = static_cast<uint16_t>(std::min(normal.y * half_max + half_max, max));
    normalz = static_cast<uint16_t>(std::min(normal.z * half_max + half_max, max));
}

void LeafNode::setMaterial(const uint16_t mat)
{
    material = mat & 0x3FF;
}

glm::vec2 LeafNode::getUV() const
{
    return { static_cast<float>(uvx) / 0xFFF, static_cast<float>(uvy) / 0xFFF };
}

glm::vec3 LeafNode::getNormal() const
{
    return { static_cast<float>(normalx) / 0x03FF, static_cast<float>(normaly) / 0x03FF, static_cast<float>(normalz) / 0x03FF };
}

uint16_t LeafNode::getMaterial(const LeafNode1 other) const
{
    return static_cast<uint16_t>((material & 0x003) << 8 | (other.material & 0xFF));
}

uint64_t LeafNode::toRaw() const
{
    uint64_t raw = 0;
    raw |= static_cast<uint64_t>(uvx) << 52;
    raw |= static_cast<uint64_t>(uvy) << 40;
    raw |= static_cast<uint64_t>(material) << 30;
    raw |= static_cast<uint64_t>(normalx) << 20;
    raw |= static_cast<uint64_t>(normaly) << 10;
    raw |= static_cast<uint64_t>(normalz);
    return raw;
}

std::pair<LeafNode1, LeafNode2> LeafNode::split() const
{
    const uint64_t raw = toRaw();
    const uint32_t raw1 = static_cast<uint32_t>((raw & 0xFFFFFFFF00000000) >> 32);
    const uint32_t raw2 = static_cast<uint32_t>(raw & 0x00000000FFFFFFFF);
    return {LeafNode1{raw1}, LeafNode2{raw2}};
}

LeafNode LeafNode::combine(const LeafNode1 leafNode1, const LeafNode2 leafNode2)
{
    uint64_t raw = 0;
    raw |= leafNode2.toRaw();
    raw |= static_cast<uint64_t>(leafNode1.toRaw()) << 32;
    return LeafNode{raw};
}

LeafNode1::LeafNode1(const uint32_t raw)
{
    uvx =      (raw & 0xFFF00000) >> 20;
    uvy =      (raw & 0x000FFF00) >> 8;
    material = (raw & 0x000000FF);
}

void LeafNode1::setUV(const glm::vec2& uv)
{
    constexpr uint16_t max = 0xFFF;
    uvx = std::min(static_cast<uint16_t>(uv.x * max), max);
    uvy = std::min(static_cast<uint16_t>(uv.y * max), max);
}

void LeafNode1::set(const uint16_t mat)
{
    material = mat & 0x0FF;
}

glm::vec2 LeafNode1::getUV() const
{
    return { static_cast<float>(uvx) / 0xFFF, static_cast<float>(uvy) / 0xFFF };
}

uint16_t LeafNode1::getMaterial(const LeafNode2 other) const
{
    return static_cast<uint16_t>((other.material &  0x003) << 8 | (material & 0x0FF));
}

uint32_t LeafNode1::toRaw() const
{
    return uvx << 20 | uvy << 8 | material;
}

LeafNode2::LeafNode2(const uint32_t raw)
{
    material = (raw & 0xC0000000) >> 30;
    normalx =  (raw & 0x3FF00000) >> 20;
    normaly =  (raw & 0x000FFC00) >> 10;
    normalz =   raw & 0x000003FF;
}

void LeafNode2::setNormal(const glm::vec3& normal)
{
    constexpr float max = 0x3FF;
    constexpr uint16_t half_max = 0x200;
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normaly = static_cast<uint16_t>(std::min(normal.y * half_max + half_max, max));
    normalz = static_cast<uint16_t>(std::min(normal.z * half_max + half_max, max));
}

void LeafNode2::setMaterial(const uint16_t mat)
{
    this->material = mat & 0x0300;
}

glm::vec3 LeafNode2::getNormal() const
{
    constexpr uint16_t half_max = 0x1FF;
    return {
        (static_cast<float>(normalx) / half_max - 1.f),
        (static_cast<float>(normaly) / half_max - 1.f),
        (static_cast<float>(normalz) / half_max - 1.f)
    };
}

uint16_t LeafNode2::getMaterial(const LeafNode1 other) const
{
    return static_cast<uint16_t>((material & 0x0003) << 8 | (other.material & 0x00FF));
}

uint32_t LeafNode2::toRaw() const
{
    return (material & 0x0003) << 30 | normalx << 20 | normaly << 10 | normalz;
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
    case Type::LEAF_NODE1:
        data.leafNode1 = LeafNode1(0);
        break;
    case Type::LEAF_NODE2:
        data.leafNode2 = LeafNode2(0);
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
    case Type::LEAF_NODE1:
        data.leafNode1 = LeafNode1(raw);
        break;
    case Type::LEAF_NODE2:
        data.leafNode2 = LeafNode2(raw);
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
    case Type::LEAF_NODE1:
        return data.leafNode1.toRaw();
    case Type::LEAF_NODE2:
        return data.leafNode2.toRaw();
    case Type::FAR_NODE:
        return data.farNode.toRaw();
    }
    return 0;
}

std::string DebugOctreeNode::toString(const uint32_t position, const Type type) const
{
    std::stringstream ss{};
    ss << "(" << position << ")";
    // add raw data to string as hex
    ss << "[" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << pack(type) << "] ";
    switch (type)
    {
    case BRANCH_NODE:
        ss << "BranchNode: {";
        ss << "ptr: " << data.branchNode.ptr.getPtr() << " (absolute: " + std::to_string(data.branchNode.ptr.getPtr() + position) + "), ";
        ss << "isFar: " << data.branchNode.ptr.isFar() << ", ";
        ss << "leafMask: " << std::bitset<8>(data.branchNode.leafMask.toRaw()) << ", ";
        ss << "childMask: " << std::bitset<8>(data.branchNode.childMask.toRaw()) << " }";
        break;
    case LEAF_NODE1:
        ss << "LeafNode1: {";
        ss << "uv: " << glm::to_string(data.leafNode1.getUV()) << ", ";
        ss << "material: " << data.leafNode1.material << " }";
        break;
    case LEAF_NODE2:
        ss << "LeafNode2: {";
        ss << "material: " << data.leafNode2.material << ", ";
        ss << "normal: " << glm::to_string(data.leafNode2.getNormal()) << " }";
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
    : m_depth(maxDepth)
{

}

Octree::Octree(const uint8_t maxDepth, const std::string_view outputFile)
    : m_depth(maxDepth), m_dumpFile(outputFile)
{

}

uint32_t Octree::getRaw(uint32_t index) const
{
#ifdef DEBUG_STRUCTURE
    return m_data[index].pack(m_types[index]);
#else
    return m_data[index];
#endif
}

MaterialProperties& Octree::getMaterialProps(const uint32_t index)
{
    return m_materials[index];
}

const std::vector<std::string>& Octree::getMaterialTextures() const
{
    return m_materialTextures;
}

uint32_t Octree::getSize() const
{
    return static_cast<uint32_t>(m_data.size());
}

uint32_t Octree::getByteSize() const
{
    return static_cast<uint32_t>(m_data.size()) * sizeof(uint32_t);
}

uint32_t Octree::getMaterialSize() const
{
    return m_materials.size();
}

uint32_t Octree::getMaterialByteSize() const
{
    return static_cast<uint32_t>(m_materials.size()) * sizeof(MaterialProperties);
}

uint8_t Octree::getDepth() const
{
    return m_depth;
}

bool Octree::isReversed() const
{
    return m_reversed;
}

OctreeStats Octree::getStats() const
{
    return m_stats;
}

bool Octree::isOctreeLoadedFromFile() const
{
    return m_loadedFromFile;
}

void Octree::preallocate(const size_t size)
{
    m_data.reserve(size);
}

void Octree::generate(const AABB root, const ProcessFunc func, void* processData)
{
    Logger::pushContext("Octree generation");
    m_data.clear();
    m_stats = OctreeStats{};
    m_loadedFromFile = false;
    m_reversed = true;
    m_process = func;
    const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    populate(root, processData);
    const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    m_stats.constructionTime = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.f;

    Logger::print("Octree stats:\n", Logger::INFO);
    Logger::print("  Nodes: " + std::to_string(getSize()), Logger::INFO);
    Logger::print("  Voxel nodes: " + std::to_string(m_stats.voxels), Logger::INFO);
    Logger::print("  Far pointers: " + std::to_string(m_stats.farPtrs), Logger::INFO);
    Logger::print("  Construction time: " + std::to_string(m_stats.constructionTime) + "s", Logger::INFO);
    Logger::popContext();
}

NodeRef Octree::populateRec(const AABB nodeShape, const uint8_t currentDepth, void* processData)
{
    NodeRef ref = m_process(nodeShape, currentDepth, m_depth, processData);
    if (!ref.exists || ref.isLeaf)
        return ref;

    BranchNode node{0};

    // Recurse into children
    std::array<NodeRef, 8> children;
    for (int8_t i = 7; i >= 0; i--)
    {
        AABB childShape = nodeShape;
        childShape.halfSize *= 0.5f;
        childShape.center += childPositions[i] * childShape.halfSize;
        children[i] = populateRec(childShape, currentDepth + 1, processData);
        if (currentDepth == 0)
            Logger::print("Finished processing root child " + std::to_string(i), Logger::INFO);

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
            if (children[i].isLeaf)
            {
                children[i].pos++;
                validChildCount++;
                continue;
            }
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
        m_stats.farPtrs++;
        addresses[i] = children[i].pos - getSize();
        addNode(ptr);
    }

    //Push children to octree
    for (int8_t i = 7; i >= 0; i--)
    {
        if (!children[i].exists) continue;
        if (children[i].isLeaf)
        {
            addNode(LeafNode2(children[i].data2));
            addNode(LeafNode1(children[i].data1));
            m_stats.voxels += 2;
        }
        else
        {
            BranchNode child = BranchNode(children[i].data1);
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
    ref.data1 = node.toRaw();
    return ref;
}

void Octree::addNode(const BranchNode child)
{
#ifdef DEBUG_STRUCTURE
    DebugOctreeNode node;
    node.data.branchNode = child;
    m_data.push_back(node);
    m_types.push_back(Type::BRANCH_NODE);
#else
    m_data.push_back(child.toRaw());
#endif
}

void Octree::addNode(const LeafNode child)
{
    const auto [leaf1, leaf2] = child.split();
    addNode(leaf1);
    addNode(leaf2);
}

void Octree::addNode(const LeafNode1 child)
{
#ifdef DEBUG_STRUCTURE
    DebugOctreeNode node;
    node.data.leafNode1 = child;
    m_data.push_back(node);
    m_types.push_back(Type::LEAF_NODE1);
#else
    m_data.push_back(child.toRaw());
#endif
}

void Octree::addNode(const LeafNode2 child)
{
#ifdef DEBUG_STRUCTURE
    DebugOctreeNode node;
    node.data.leafNode2 = child;
    m_data.push_back(node);
    m_types.push_back(Type::LEAF_NODE2);
#else
    m_data.push_back(child.toRaw());
#endif
}

void Octree::addNode(const FarNode child)
{
#ifdef DEBUG_STRUCTURE
    DebugOctreeNode node;
    node.data.farNode = child;
    m_data.push_back(node);
    m_types.push_back(Type::FAR_NODE);
#else
    m_data.push_back(child.toRaw());
#endif
}

void Octree::updateNode(uint32_t index, BranchNode node)
{
#ifdef DEBUG_STRUCTURE
    m_data[index].update(node.toRaw(), Type::BRANCH_NODE);
#else
    m_data[index] = node.toRaw();
#endif
}

void Octree::updateNode(uint32_t index, LeafNode1 node)
{
#ifdef DEBUG_STRUCTURE
    m_data[index].update(node.toRaw(), Type::LEAF_NODE1);
#else
    m_data[index] = node.toRaw();
#endif
}

void Octree::updateNode(uint32_t index, LeafNode2 node)
{
#ifdef DEBUG_STRUCTURE
    m_data[index].update(node.toRaw(), Type::LEAF_NODE2);
#else
    m_data[index] = node.toRaw();
#endif
}

void Octree::updateNode(uint32_t index, FarNode node)
{
#ifdef DEBUG_STRUCTURE
    m_data[index].update(node.toRaw(), Type::FAR_NODE);
#else
    m_data[index] = node.toRaw();
#endif
}

void* Octree::getData()
{
    return m_data.data();
}

void* Octree::getMaterialData()
{
    return m_materials.data();
}

void* Octree::getMaterialTexData()
{
    return m_materials.data();
}

void Octree::dump(const std::string_view filenameArg) const
{
    Logger::pushContext("Octree dumping");
    const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    const std::string filename = filenameArg.empty() ? m_dumpFile : filenameArg.data();
    if (filename.empty())
    {
        Logger::print("No filename provided for octree dumping", Logger::ERR);
        return;
    }
    std::ofstream file(filename.data(), std::ios::binary);
    const size_t size = getSize();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(&m_stats.voxels), sizeof(m_stats.voxels));
    file.write(reinterpret_cast<const char*>(&m_stats.farPtrs), sizeof(m_stats.farPtrs));
    file.write(reinterpret_cast<const char*>(&m_stats.materials), sizeof(m_stats.materials));
    file.write(reinterpret_cast<const char*>(&m_stats.constructionTime), sizeof(m_stats.constructionTime));
    if (!m_reversed)
        file.write(reinterpret_cast<const char*>(m_data.data()), getByteSize());
    else
    {
        for (uint32_t i = 1; i <= m_data.size(); i++)
        {
#ifdef DEBUG_STRUCTURE
            const uint32_t raw = m_data[getSize() - i].pack(m_types[getSize() - i]);
#else
            const uint32_t raw = m_data[getSize() - i];
#endif
            file.write(reinterpret_cast<const char*>(&raw), sizeof(raw));
        }
    }
    const size_t matSize = getMaterialSize();
    file.write(reinterpret_cast<const char*>(&matSize), sizeof(matSize));
    file.write(reinterpret_cast<const char*>(m_materials.data()), getMaterialByteSize());
    const size_t texSize = m_materialTextures.size();
    file.write(reinterpret_cast<const char*>(&texSize), sizeof(texSize));
    for (const std::string& texture : m_materialTextures)
    {
        const uint32_t texSize = static_cast<uint32_t>(texture.size());
        file.write(reinterpret_cast<const char*>(&texSize), sizeof(texSize));
        file.write(texture.data(), texSize);
    }
    file.close();
    const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    m_stats.saveTime = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.f;
    Logger::popContext();
}

void Octree::load(const std::string_view filename)
{
    Logger::pushContext("Octree loading");
    m_data.clear();
    m_stats = OctreeStats{};
    const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    if (filename.empty())
    {
        if (m_dumpFile.empty())
        {
            Logger::print("No filename provided for octree loading", Logger::ERR);
            return;
        }
        load(m_dumpFile);
        return;
    }
    std::ifstream file(filename.data(), std::ios::binary);
    size_t size;
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    file.read(reinterpret_cast<char*>(&m_stats.voxels), sizeof(m_stats.voxels));
    file.read(reinterpret_cast<char*>(&m_stats.farPtrs), sizeof(m_stats.farPtrs));
    file.read(reinterpret_cast<char*>(&m_stats.materials), sizeof(m_stats.materials));
    file.read(reinterpret_cast<char*>(&m_stats.constructionTime), sizeof(m_stats.constructionTime));
    m_data.resize(size);
    file.read(reinterpret_cast<char*>(m_data.data()), getByteSize());
    size_t matSize;
    file.read(reinterpret_cast<char*>(&matSize), sizeof(matSize));
    m_materials.resize(matSize);
    file.read(reinterpret_cast<char*>(m_materials.data()), getMaterialByteSize());
    size_t texSize;
    file.read(reinterpret_cast<char*>(&texSize), sizeof(texSize));
    m_materialTextures.resize(texSize);
    for (uint32_t i = 0; i < texSize; i++)
    {
        uint32_t pathSize;
        file.read(reinterpret_cast<char*>(&pathSize), sizeof(pathSize));
        m_materialTextures[i].resize(pathSize);
        file.read(m_materialTextures[i].data(), pathSize);
    }
    file.close();
    const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    m_stats.saveTime = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.f;
    m_loadedFromFile = true;
    Logger::popContext();
}

void Octree::setMaterialPath(const std::string_view path)
{
    m_textureRootDir = path;
    if (path.back() != '/') m_textureRootDir += '/';
}

void Octree::addMaterial(MaterialProperties material, const std::string_view albedoMap, const std::string_view normalMap)
{
    std::string albedoPath = m_textureRootDir + albedoMap.data();
    std::string normalPath = m_textureRootDir + normalMap.data();
    for (uint32_t i = 0; i < m_materialTextures.size(); i++)
    {
        if (m_materialTextures[i] == albedoPath)
            material.albedoMap = i;
        else if (m_materialTextures[i] == normalPath)
            material.normalMap = i;
    }
    if (material.albedoMap == 500 && !albedoMap.empty())
    {
        m_materialTextures.emplace_back(albedoPath.data());
        material.albedoMap = static_cast<uint16_t>(m_materialTextures.size() - 1);
    }
    if (material.normalMap == 500 && !normalMap.empty())
    {
        m_materialTextures.emplace_back(normalPath.data());
        material.normalMap = static_cast<uint16_t>(m_materialTextures.size() - 1);
    }
    m_materials.push_back(material);
}

void Octree::packAndFinish()
{
    if (m_materials.empty())
        m_materials.push_back(MaterialProperties{});
    m_finished = true;
    m_stats.materials = static_cast<uint16_t>(m_materials.size());
}

void Octree::clear()
{
    m_data.clear();
    m_depth = 0;
}

bool Octree::isFinished() const
{
    return m_finished;
}

void Octree::populate(const AABB nodeShape, void* processData)
{
    const NodeRef ref = populateRec(nodeShape, 0, processData);
    if (!ref.exists)
    {
        Logger::print("Octree generation returned non-existent root. Resulting octree is empty or broken", Logger::WARN);
        return;
    }
    if (ref.isLeaf)
    {
        addNode(LeafNode2(ref.data2));
        addNode(LeafNode1(ref.data1));
    }
    else
    {
        BranchNode node{ref.data1};
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
    for (uint32_t i = 1; i <= m_data.size(); i++)
    {
        ss << m_data[getSize() - i].toString(i - 1, m_types[getSize() - i]) << '\n';
    }
    return ss.str();
}

Type Octree::getType(const uint32_t index) const
{
    return m_types[index];
}

uint32_t Octree::get(const uint32_t index) const
{
    const Type type = m_types[index];
    switch (type)
    {
    case Type::BRANCH_NODE:
        return m_data[index].data.branchNode.toRaw();
    case Type::LEAF_NODE1:
        return m_data[index].data.leafNode1.toRaw();
    case Type::LEAF_NODE2:
        return m_data[index].data.leafNode2.toRaw();
    case Type::FAR_NODE:
        return m_data[index].data.farNode.toRaw();
}
    return 0;
}
#else
uint32_t& Octree::get(const uint32_t index)
{
    return m_data[index];
}
#endif
