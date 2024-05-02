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

Octree::Material& Octree::getMaterialProps(const uint32_t index)
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
    return static_cast<uint32_t>(m_materials.size()) * sizeof(Material);
}

uint8_t Octree::getDepth() const
{
    return m_depth;
}

bool Octree::isReversed() const
{
    return m_reversed;
}

Octree::Stats Octree::getStats() const
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
    m_stats = Stats{};
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
    m_stats = Stats{};
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

void Octree::addMaterial(Material material, const std::string_view diffuseMap, const std::string_view normalMap, const std::string_view specularMap)
{
    std::string diffusePath = m_textureRootDir + diffuseMap.data();
    std::string normalPath = m_textureRootDir + normalMap.data();
    std::string specularPath = m_textureRootDir + specularMap.data();
    for (uint32_t i = 0; i < m_materialTextures.size(); i++)
    {
        if (m_materialTextures[i] == diffusePath)
            material.diffuseMap = i;
        if (m_materialTextures[i] == normalPath)
            material.normalMap = i;
        if (m_materialTextures[i] == specularPath)
            material.specularMap = i;
    }
    if (material.diffuseMap == 500 && !diffuseMap.empty())
    {
        m_materialTextures.emplace_back(diffusePath.data());
        material.diffuseMap = static_cast<uint16_t>(m_materialTextures.size() - 1);
    }
    if (material.normalMap == 500 && !normalMap.empty())
    {
        m_materialTextures.emplace_back(normalPath.data());
        material.normalMap = static_cast<uint16_t>(m_materialTextures.size() - 1);
    }
    if (material.specularMap == 500 && !specularMap.empty())
    {
        m_materialTextures.emplace_back(specularPath.data());
        material.specularMap = static_cast<uint16_t>(m_materialTextures.size() - 1);
    }
    m_materials.push_back(material);
}

void Octree::packAndFinish()
{
    if (m_materials.empty())
        m_materials.push_back(Material{});
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
