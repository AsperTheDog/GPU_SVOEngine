#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "octree_nodes.hpp"

enum { NEAR_PTR_MAX = 0x7FFF };

struct NodeRef
{
    uint32_t data1 = 0;
    uint32_t data2 = 0;
    uint32_t pos = 0;
    uint32_t childPos = 0;
    bool isLeaf = false;
    bool exists = false;
};

struct AABB
{
    glm::vec3 center;
    float halfSize;
};

struct FarNodeRef
{
    uint32_t sourcePos;
    uint32_t destinationPos;
    uint32_t farNodePos;
};

// Statistics data

typedef NodeRef(*ProcessFunc)(const AABB&, uint8_t, uint8_t, void*);
typedef NodeRef(*ParallelProcessFunc)(const AABB&, uint8_t, uint8_t, void*, uint8_t);

class Octree
{
public:
    struct Material
    {
        alignas(16) glm::vec3 ambient = {1.0, 1.0, 1.0};
        alignas(16) glm::vec3 diffuse = {1.0, 1.0, 1.0};
        alignas(16) glm::vec3 specular = {1.0, 1.0, 1.0};
        alignas(4) float specularComp = 0.0;
        alignas(4) uint32_t diffuseMap = 500;
        alignas(4) uint32_t normalMap = 500;
        alignas(4) uint32_t specularMap = 500;
    };

    struct Stats
    {
        uint64_t voxels = 0;
        uint64_t farPtrs = 0;
        uint16_t materials = 0;
        float constructionTime = 0;
        float saveTime = 0;
    };

    explicit Octree(uint8_t maxDepth);
    Octree (uint8_t maxDepth, std::string_view outputFile);

    [[nodiscard]] uint32_t getRaw(uint32_t index) const;
    [[nodiscard]] Material& getMaterialProps(uint32_t index);
    [[nodiscard]] const std::vector<std::string>& getMaterialTextures() const;

    [[nodiscard]] uint32_t getSize() const;
    [[nodiscard]] uint32_t getByteSize() const;
    [[nodiscard]] uint32_t getMaterialSize() const;
    [[nodiscard]] uint32_t getMaterialByteSize() const;
    [[nodiscard]] uint8_t getDepth() const;
    [[nodiscard]] bool isReversed() const;
    [[nodiscard]] Stats getStats() const;
    [[nodiscard]] bool isOctreeLoadedFromFile() const;
    [[nodiscard]] bool isFinished() const;

    void preallocate(size_t size);
    void generate(AABB root, ProcessFunc func, void* processData);
    void generateParallel(AABB rootShape, ParallelProcessFunc func, void* processData);
    void addNode(BranchNode child);
    void addNode(LeafNode child);
    void addNode(LeafNode1 child);
    void addNode(LeafNode2 child);
    void addNode(FarNode child);
    void updateNode(uint32_t index, BranchNode node);
    void updateNode(uint32_t index, LeafNode1 node);
    void updateNode(uint32_t index, LeafNode2 node);
    void updateNode(uint32_t index, FarNode node);
    void dump(std::string_view filename) const;

    void* getData();
    void* getMaterialData();
    void* getMaterialTexData();
    void load(std::string_view filename = "");

    void setMaterialPath(std::string_view path);
    void addMaterial(Material material, std::string_view diffuseMap, std::string_view normalMap, std::string_view specularMap);
    void packAndFinish();

    void clear();

private:
    void populate(AABB nodeShape, void* processData, bool parallel, uint8_t parallelIndex = 0);
    NodeRef populateRec(AABB nodeShape, uint8_t currentDepth, void* processData, bool parallel, uint8_t parallelIndex);

    void resolveFarPointersAndPush(std::array<NodeRef, 8>& children);
    void resolveRoot(const NodeRef& ref);

    std::vector<uint32_t> m_data;
    uint32_t& get(uint32_t index);

    std::vector<Material> m_materials;
    std::vector<std::string> m_materialTextures;
    std::string m_textureRootDir;

    size_t m_sizePtr = 0;
    uint8_t m_depth = 0;
    ProcessFunc m_process = nullptr;
    ParallelProcessFunc m_parallelProcess = nullptr;
    std::string m_dumpFile;

    bool m_reversed = false;
    bool m_loadedFromFile = false;
    bool m_finished = false;

    mutable Stats m_stats{};
};

