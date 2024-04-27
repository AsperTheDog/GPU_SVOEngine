#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

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
    explicit LeafNode(uint32_t raw);


    uint8_t colorz : 4;
    uint8_t colory : 4;
    uint8_t colorx : 4;
    uint8_t normalz : 4;
    uint8_t normaly : 4;
    uint8_t normalx : 4;
    uint8_t material;

    void setNormal(const glm::u8vec3& normal);
    void setColor(const glm::u8vec3& color);

    [[nodiscard]] glm::u8vec3 getNormal() const;
    [[nodiscard]] glm::u8vec3 getColor() const;

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
    LEAF_NODE = 1,
    FAR_NODE = 2
};

struct DebugOctreeNode
{
    union Data
    {
        BranchNode branchNode;
        LeafNode leafNode;
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
    uint32_t data = 0;
    uint32_t pos = 0;
    uint32_t childPos = 0;
    bool isLeaf = false;
    bool exists = false;
};

struct AABB
{
    glm::vec3 center;
    glm::vec3 halfSize;
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
    void generate(ProcessFunc func, void* processData);
    void addNode(BranchNode child);
    void addNode(LeafNode child);
    void addNode(FarNode child);
    void updateNode(uint32_t index, BranchNode node);
    void updateNode(uint32_t index, LeafNode node);
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
    std::vector<DebugOctreeNode> data;
    std::vector<Type> types;
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

