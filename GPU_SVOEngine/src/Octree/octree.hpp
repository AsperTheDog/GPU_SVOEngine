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

private:
    uint8_t field;
};

// Octree classes

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

struct FarNodeRef
{
    uint32_t sourcePos;
    uint32_t destinationPos;
    uint32_t farNodePos;
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

    explicit DebugOctreeNode(Type type);
    void update(uint32_t raw, Type type);
    [[nodiscard]] uint32_t pack(Type type) const;
    [[nodiscard]] std::string toString(uint32_t position, Type type) const;
};
#endif

class Octree
{
public:
    [[nodiscard]] uint32_t getSize() const;
    [[nodiscard]] uint32_t getByteSize() const;

    [[nodiscard]] uint8_t getDepth() const;

    void preallocate(size_t size);
    void generateRandomly(uint8_t maxDepth, float density);
    void addNode(BranchNode child);
    void addNode(LeafNode child);
    void addNode(FarNode child);
    void updateNode(uint32_t index, BranchNode node);
    void updateNode(uint32_t index, LeafNode node);
    void updateNode(uint32_t index, FarNode node);
    void pack();
    NearPtr pushFarPtr(uint32_t sourcePos, uint32_t destinationPos, uint32_t farNodePos);

    void* getData();
    void dump(const std::string& filename) const;

    void clear();

private:
    bool populateRec(unsigned parentPos, unsigned char currentDepth, unsigned char maxDepth, glm::vec3 color, float density);

#ifdef DEBUG_STRUCTURE
    std::vector<DebugOctreeNode> data;
    std::vector<Type> types;
    [[nodiscard]] uint32_t get(uint32_t index) const;
#else
    std::vector<uint32_t> data;
    uint32_t& get(uint32_t index);
#endif
    std::vector<FarNodeRef> farPtrs;
    uint8_t depth = 0;
};

