#pragma once

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
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float specularComp;
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

struct TriangleIndex
{
    uint32_t index : 31 = 0;
    uint32_t confined : 1 = 0;

    TriangleIndex(uint32_t index, bool confined);
};

struct TriangleLeafIndex
{
    float d = 0;
    glm::vec2 baricentric{0, 0};
    bool hit = false;
    TriangleIndex index{0, false};
};

// VOXELIZER

class Voxelizer
{
public:
    explicit Voxelizer(std::string filename, uint8_t maxDepth);
    [[nodiscard]] TriangleLeafIndex AABBTriangle6Connect(TriangleIndex index, AABB shape) const;

    static bool intersectAABBTriangleSAT(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, AABB shape);
    static bool intersectAABBPoint(glm::vec3 point, AABB shape);

    bool doesAABBInteresect(const AABB& shape, bool isLeaf, uint8_t depth);
    void sampleVoxel(NodeRef& node) const;
    [[nodiscard]] AABB getModelAABB() const;
    [[nodiscard]] const std::vector<Material>& getMaterials() const;

    [[nodiscard]] std::string getMaterialFilePath() const;

    static NodeRef voxelize(const AABB& nodeShape, const uint8_t depth, const uint8_t maxDepth, void* data);

private:
    [[nodiscard]] std::array<glm::vec3, 3> getTrianglePos(TriangleIndex triangle) const;
    [[nodiscard]] Triangle getTriangle(TriangleIndex triangle) const;
    [[nodiscard]] Material getMaterial(TriangleIndex triangle) const;
    [[nodiscard]] uint16_t getMaterialID(TriangleIndex triangle) const;
    [[nodiscard]] std::array<glm::vec3, 3> getTrianglePos(TriangleRootIndex rootIndex) const;
    [[nodiscard]] Triangle getTriangle(TriangleRootIndex rootIndex) const;
    [[nodiscard]] Material getMaterial(TriangleRootIndex rootIndex) const;

    Model m_model;

    std::vector<TriangleRootIndex> m_triangles;
    std::vector<std::vector<TriangleIndex>> m_triangleTree;
    std::vector<TriangleLeafIndex> m_triangleLeafs;

    std::unordered_map<uint32_t, glm::u8vec3> m_colorMap;

    std::string m_baseDir;
};

