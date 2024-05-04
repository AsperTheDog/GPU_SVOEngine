#pragma once

#include <array>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "octree.hpp"

// MODEL DATA

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 texCoord;
    glm::vec3 normal;

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && texCoord == other.texCoord && normal == other.normal;
    }
};

template<> struct std::hash<Vertex>
{
	size_t operator()(Vertex const& vertex) const noexcept
	{
		return hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct Material
{
    std::string name;
    glm::vec3 ambient{1.0f, 1.0f, 1.0f};
    glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};
    float specularComp = 0;
    std::string diffuseMap;
    std::string normalMap;
    std::string specularMap;

    [[nodiscard]] Octree::Material toOctreeMaterial() const;
};

struct Model
{
    std::vector<Mesh> meshes;
    std::vector<Material> materials;

    glm::vec3 min{FLT_MAX};
    glm::vec3 max{FLT_MIN};
};

// VOXELIZER HELPER STRUCTS

struct Triangle
{
    Vertex v0;
    Vertex v1;
    Vertex v2;
    

    struct WeightData
    {
        glm::vec3 weights;
        glm::vec3 position;
    };

    [[nodiscard]] glm::vec3 getInterpolationWeights(glm::vec3 point) const;
    [[nodiscard]] WeightData getTriangleClosestWeight(glm::vec3 point) const;
    [[nodiscard]] glm::vec2 getWeightedUV(glm::vec3 weights) const;
    [[nodiscard]] glm::vec3 getWeightedNormal(glm::vec3 weights) const;
};

struct TriangleRootIndex
{
    uint16_t meshIndex;
    uint32_t index;
};

struct TriangleLeafIndex
{
    float d = 0;
    glm::vec2 baricentric{0, 0};
    bool hit = false;
    uint32_t index = 0;
};

// VOXELIZER

struct OctreeAccStructure
{
    std::vector<std::vector<uint32_t>> m_triangleTree;
    std::vector<TriangleLeafIndex> m_triangleLeafs;
};

class Voxelizer
{
public:
    explicit Voxelizer(std::string filename, uint8_t maxDepth);
    [[nodiscard]] TriangleLeafIndex AABBTriangle6Connect(uint32_t index, AABB shape) const;

    static bool intersectAABBTriangleSAT(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, AABB shape);
    static bool intersectAABBPoint(glm::vec3 point, AABB shape);

    bool doesAABBInteresect(const AABB& shape, bool isLeaf, uint8_t depth, uint8_t parallelIndex);
    void sampleVoxel(NodeRef& node, uint8_t parallelIndex) const;
    [[nodiscard]] AABB getModelAABB() const;
    [[nodiscard]] const std::vector<Material>& getMaterials() const;

    [[nodiscard]] std::string getMaterialFilePath() const;

    static NodeRef voxelize(const AABB& nodeShape, uint8_t depth, uint8_t maxDepth, void* data);
    static NodeRef parallelVoxelize(const AABB& nodeShape, uint8_t depth, uint8_t maxDepth, void* data, uint8_t parallelIndex);

    void resetOctreeData(uint8_t newDepth);

private:
    [[nodiscard]] std::array<glm::vec3, 3> getTrianglePos(uint32_t triangle) const;
    [[nodiscard]] Triangle getTriangle(uint32_t triangle) const;
    [[nodiscard]] Material getMaterial(uint32_t triangle) const;
    [[nodiscard]] uint16_t getMaterialID(uint32_t triangle) const;
    [[nodiscard]] std::array<glm::vec3, 3> getTrianglePos(TriangleRootIndex rootIndex) const;
    [[nodiscard]] Triangle getTriangle(TriangleRootIndex rootIndex) const;
    [[nodiscard]] Material getMaterial(TriangleRootIndex rootIndex) const;

    Model m_model;

    std::vector<TriangleRootIndex> m_triangles;
    struct TriangleTree
    {
        std::vector<std::vector<uint32_t>> branchTriangles{};
        std::vector<TriangleLeafIndex> leafTriangles{};
    };
    std::array<TriangleTree, 8> m_triangleTrees;
    std::vector<uint32_t> m_rootTriangles;


    std::string m_baseDir;
};

