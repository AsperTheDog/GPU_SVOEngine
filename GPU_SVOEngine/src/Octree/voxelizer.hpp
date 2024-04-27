#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "octree.hpp"

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
    uint32_t materialIndex;
};

struct Material
{
    glm::vec3 color;
    float roughness;
    float metallic;
    float ior;
    float transmission;
    float emission;
    std::string albedoMap;
    std::string normalMap;
};

struct Model
{
    std::vector<Mesh> meshes;
    std::vector<Material> materials;

    glm::vec3 min{FLT_MAX};
    glm::vec3 max{FLT_MIN};

    uint32_t getMesh(uint32_t material);
};

struct Triangle
{
    Vertex v0;
    Vertex v1;
    Vertex v2;
};

struct TriangleRootIndex
{
    uint32_t matIndex;
    uint32_t index;

    [[nodiscard]] uint32_t getMaterial() const;
    [[nodiscard]] uint32_t getIndex() const;
};

struct TriangleIndex
{
    uint32_t index : 31 = 0;
    uint32_t confined : 1 = 0;

    [[nodiscard]] uint32_t getIndex() const;
    [[nodiscard]] bool isConfined() const;

    TriangleIndex(uint32_t index, bool confined);
};

class Voxelizer
{
public:
    explicit Voxelizer(std::string filename, uint8_t maxDepth);

    static bool intersectAABBTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, const AABB shape);
    static bool intersectAABBPoint(glm::vec3 point, const AABB shape);

    bool doesAABBInteresect(const AABB& shape, bool isLeaf, uint8_t depth);
    void sampleVoxel(NodeRef& node, uint8_t depth) const;
    [[nodiscard]] AABB getModelAABB() const;

private:
    [[nodiscard]] std::array<glm::vec3, 3> getTrianglePos(TriangleIndex triangle) const;
    [[nodiscard]] Triangle getTriangle(TriangleIndex triangle) const;
    [[nodiscard]] Material getMaterial(TriangleIndex triangle) const;
    [[nodiscard]] uint32_t getMaterialID(TriangleIndex triangle) const;
    [[nodiscard]] std::array<glm::vec3, 3> getTrianglePos(TriangleRootIndex rootIndex) const;
    [[nodiscard]] Triangle getTriangle(TriangleRootIndex rootIndex) const;
    [[nodiscard]] Material getMaterial(TriangleRootIndex rootIndex) const;

    Model model;

    std::vector<TriangleRootIndex> triangles;
    std::vector<std::vector<TriangleIndex>> triangleTree;

    std::unordered_map<uint32_t, glm::u8vec3> colorMap;
};

