#pragma once
#include "voxelizer.hpp"


struct RandomData
{
    float density = 0.0f;
    std::vector<glm::vec3> color{};
};

inline glm::vec3 shiftColor(const glm::vec3 color)
{
    float r = std::min(std::max(color.r + (rand() % 2 == 0 ? 0.8f : -0.8f), 0.0f), 15.0f);
    float g = std::min(std::max(color.g + (rand() % 2 == 0 ? 0.8f : -0.8f), 0.0f), 15.0f);
    float b = std::min(std::max(color.b + (rand() % 2 == 0 ? 0.8f : -0.8f), 0.0f), 15.0f);
    return { r, g, b };
}

inline NodeRef generateRandomly(const AABB& nodeShape, const uint8_t depth, const uint8_t maxDepth, void* data)
{
    NodeRef nodeRef{};
    if (depth == 0)
    {
        nodeRef.exists = true;
        nodeRef.isLeaf = false;
        return nodeRef;
    }

    RandomData& randomData = *static_cast<RandomData*>(data);

    nodeRef.exists = static_cast<float>(rand()) / RAND_MAX < randomData.density;
    if (!nodeRef.exists) return nodeRef;
    nodeRef.isLeaf = depth >= maxDepth;

    randomData.color[depth] = shiftColor(randomData.color[depth - 1]);

    if (nodeRef.isLeaf)
    {
        LeafNode leafNode{ 0 };
        leafNode.setUV({nodeShape.center.x, nodeShape.center.y});
        leafNode.setNormal(glm::vec3{ 0.0f, 1.0f, 0.0f });
        leafNode.material = 0;
        const std::pair<LeafNode1, LeafNode2> leafs = leafNode.split();
        nodeRef.data1 = leafs.first.toRaw();
        nodeRef.data2 = leafs.second.toRaw();
    }

    return nodeRef;
}

// VOXELIZER

inline NodeRef voxelize(const AABB& nodeShape, const uint8_t depth, const uint8_t maxDepth, void* data)
{
    Voxelizer& voxelizer = *static_cast<Voxelizer*>(data);
    NodeRef nodeRef{};
    nodeRef.isLeaf = depth >= maxDepth;
    nodeRef.exists = voxelizer.doesAABBInteresect(nodeShape, nodeRef.isLeaf, depth);
    if (nodeRef.exists && nodeRef.isLeaf)
        voxelizer.sampleVoxel(nodeShape, nodeRef, depth);
    return nodeRef;
}