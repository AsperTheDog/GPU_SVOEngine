#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct LeafNode1;
struct LeafNode2;

enum { NEAR_PTR_MAX = 0x7FFF };

#ifdef _DEBUG
#define DEBUG_STRUCTURE
#endif

#ifdef DEBUG_STRUCTURE
#include <string>
#endif

// Helper classes

class NearPtr
{
public:
    explicit NearPtr(uint16_t ptr, bool isFar);

    [[nodiscard]] uint16_t getPtr() const;
    [[nodiscard]] bool isFar() const;

    [[nodiscard]] uint16_t toRaw() const;

    void setPtr(uint16_t ptr);
    void setFar(bool isFar);

private:
    uint16_t addr : 15;
    uint16_t farFlag : 1;
};

class BitField
{
public:
    explicit BitField(uint8_t field);

    [[nodiscard]] bool getBit(uint8_t index) const;
    void setBit(uint8_t index, bool value);

    [[nodiscard]] uint8_t toRaw() const;

    bool operator==(const BitField& other) const;

private:
    uint8_t field;
};

// Octree structures

struct BranchNode
{
    explicit BranchNode(uint32_t raw);

    BitField leafMask{ 0 };
    BitField childMask{ 0 };
    NearPtr ptr{ 0, false };

    [[nodiscard]] uint32_t toRaw() const;
};

struct LeafNode
{
    explicit LeafNode(uint64_t raw);

    uint64_t normalz : 10;
    uint64_t normaly : 10;
    uint64_t normalx : 10;
    uint64_t material : 10;
    uint64_t uvy : 12;
    uint64_t uvx : 12;

    void setUV(glm::vec2 uv);
    void setNormal(const glm::vec3& normal);
    void setMaterial(uint16_t material);

    [[nodiscard]] glm::vec2 getUV() const;
    [[nodiscard]] glm::vec3 getNormal() const;
    [[nodiscard]] uint16_t getMaterial(LeafNode1 other) const;

    [[nodiscard]] uint64_t toRaw() const;
    [[nodiscard]] std::pair<LeafNode1, LeafNode2> split() const;

    static LeafNode combine(LeafNode1 leafNode1, LeafNode2 leafNode2);
};

struct LeafNode1
{
    explicit LeafNode1(uint32_t raw);

    
    uint32_t material : 8;
    uint32_t uvy : 12;
    uint32_t uvx : 12;

    void setUV(const glm::vec2& uv);
    void set(uint16_t material);

    [[nodiscard]] glm::vec2 getUV() const;
    [[nodiscard]] uint16_t getMaterial(LeafNode2 other) const;

    [[nodiscard]] uint32_t toRaw() const;
};

struct LeafNode2
{
    explicit LeafNode2(uint32_t raw);

    uint32_t normalz : 10;
    uint32_t normaly : 10;
    uint32_t normalx : 10;
    uint32_t material : 2;

    void setNormal(const glm::vec3& normal);
    void setMaterial(uint16_t material);

    [[nodiscard]] glm::vec3 getNormal() const;
    [[nodiscard]] uint16_t getMaterial(LeafNode1 other) const;

    [[nodiscard]] uint32_t toRaw() const;
};

struct FarNode
{
    explicit FarNode(uint32_t raw);

    uint32_t ptr;

    [[nodiscard]] uint32_t toRaw() const;
};

#ifdef DEBUG_STRUCTURE
enum Type : uint8_t
{
    BRANCH_NODE = 0,
    LEAF_NODE1 = 1,
    LEAF_NODE2 = 2,
    FAR_NODE = 3
};

struct DebugOctreeNode
{
    union Data
    {
        BranchNode branchNode;
        LeafNode1 leafNode1;
        LeafNode2 leafNode2;
        FarNode farNode;

        Data();
    } data;

    DebugOctreeNode() = default;
    explicit DebugOctreeNode(Type type);
    void update(uint32_t raw, Type type);
    [[nodiscard]] uint32_t pack(Type type) const;
    [[nodiscard]] std::string toString(uint32_t position, Type type) const;
};
#endif

// Construction structures

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

struct OctreeStats
{
    uint64_t voxels = 0;
    uint64_t farPtrs = 0;
    float constructionTime = 0;
    float saveTime = 0;
};

typedef NodeRef(*ProcessFunc)(const AABB&, uint8_t, uint8_t, void*);

class Octree
{
public:
    explicit Octree(uint8_t maxDepth);
    Octree (uint8_t maxDepth, std::string_view outputFile);

    [[nodiscard]] uint32_t getRaw(uint32_t index) const;

    [[nodiscard]] uint32_t getSize() const;
    [[nodiscard]] uint32_t getByteSize() const;
    [[nodiscard]] uint8_t getDepth() const;
    [[nodiscard]] bool isReversed() const;
    [[nodiscard]] OctreeStats getStats() const;
    [[nodiscard]] bool isOctreeLoadedFromFile() const;

    void preallocate(size_t size);
    void generate(AABB root, ProcessFunc func, void* processData);
    void addNode(BranchNode child);
    void addNode(LeafNode child);
    void addNode(LeafNode1 child);
    void addNode(LeafNode2 child);
    void addNode(FarNode child);
    void updateNode(uint32_t index, BranchNode node);
    void updateNode(uint32_t index, LeafNode1 node);
    void updateNode(uint32_t index, LeafNode2 node);
    void updateNode(uint32_t index, FarNode node);
    void pack();
    NearPtr pushFarPtr(uint32_t sourcePos, uint32_t destinationPos, uint32_t farNodePos);

    void* getData();
    void dump(std::string_view filename) const;
    void load(std::string_view filename = "");

    void clear();

#ifdef DEBUG_STRUCTURE
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] Type getType(uint32_t index) const;
#endif

private:
    void populate(AABB nodeShape, void* processData);
    NodeRef populateRec(AABB node, uint8_t currentDepth, void* processData);

#ifdef DEBUG_STRUCTURE
    std::vector<DebugOctreeNode> m_data;
    std::vector<Type> m_types;
    [[nodiscard]] uint32_t get(uint32_t index) const;
#else
    std::vector<uint32_t> m_data;
    uint32_t& get(uint32_t index);
#endif
    std::vector<FarNodeRef> m_farPtrs;

    size_t m_sizePtr = 0;
    uint8_t m_depth = 0;
    ProcessFunc m_process = nullptr;
    std::string m_dumpFile;
    bool m_reversed = false;
    bool m_loadedFromFile = false;

    OctreeStats m_stats{};
};

