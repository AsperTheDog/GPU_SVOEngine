# pragma once
#include <cstdint>
#include <glm/glm.hpp>

#include "octree_helper.hpp"

#ifdef DEBUG_STRUCTURE // global define in Debug configuration
#include <string>
#endif

// Most structures are just helpers to create and manage the octree. The main types of nodes in the octree are:

// BranchNode:
// - Contains a NearPtr denoting the address of the children 15 bits are for the address and 1 bit is to know if it's pointer to the child or to a FarPtr
// - Contains a BitField for the child mask. Each bit denotes if the corresponding child exists
// - Contains a BitField for the leaf mask. Each bit denotes if the corresponding child is a leaf node

// LeafNode:
// - Contains 24 bits for the UV coordinates (12 bits for each axis)
// - Contains 10 bits for the material index
// - Contains 30 bits for the normal vector (10 bits for each axis)
// Since the LeafNode is 64 bits it on serialization it is split into two 32 bit nodes (LeafNode1 and LeafNode2)

// FarNode:
// - All 32 bits of the node are for the address of the next node

struct BranchNode
{
    explicit BranchNode(uint32_t raw);

    BitField leafMask{ 0 };
    BitField childMask{ 0 };
    NearPtr ptr{ 0, false };

    [[nodiscard]] uint32_t toRaw() const;
};

struct LeafNode1;
struct LeafNode2;

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