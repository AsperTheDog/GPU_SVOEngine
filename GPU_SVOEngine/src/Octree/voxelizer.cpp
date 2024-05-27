#include "voxelizer.hpp"

#include <stdexcept>

#include <array>
#include <unordered_set>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/norm.hpp>

#include "utils/logger.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


glm::vec2 Triangle::getWeightedUV(const glm::vec3 weights) const
{
    return v0.texCoord * weights.x + v1.texCoord * weights.y + v2.texCoord * weights.z;
}

glm::vec3 Triangle::getWeightedNormal(const glm::vec3 weights) const
{
    return v0.normal * weights.x + v1.normal * weights.y + v2.normal * weights.z;
}

// The constructor loads the model data and materials from the file
Voxelizer::Voxelizer(std::string filename, uint8_t maxDepth)
{
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        m_baseDir = filename.substr(0, filename.find_last_of('/'));

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.data(), m_baseDir.c_str(), true))
        {
            throw std::runtime_error(warn + err);
        }

        m_model.materials.emplace_back();
        for (tinyobj::material_t& material : materials)
        {
            Material mat{};
            mat.name = material.name;
            mat.diffuse = { material.diffuse[0], material.diffuse[1], material.diffuse[2] };
            mat.ambient = { material.ambient[0], material.ambient[1], material.ambient[2] };
            mat.specular = { material.specular[0], material.specular[1], material.specular[2] };
            mat.specularComp = material.shininess;
            mat.diffuseMap = material.diffuse_texname;
            mat.normalMap = material.normal_texname;
            mat.specularMap = material.specular_texname;

            m_model.materials.push_back(mat);
        }

        m_model.meshes.resize(m_model.materials.size());

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};
        Mesh* mesh = nullptr;
        int materialIndex = INT32_MIN;
        for (const tinyobj::shape_t& shape : shapes)
        {
            for (uint32_t i = 0; i < shape.mesh.indices.size(); i++)
            {
                // If the material index is different from the previous one, we change the mesh
                // We either create a new mesh or use the one that has the same material index
                if (shape.mesh.material_ids[i / 3] + 1 != materialIndex)
                {
                    materialIndex = shape.mesh.material_ids[i / 3] + 1;
                    mesh = &m_model.meshes[materialIndex];
                    uniqueVertices.clear();
                }
                Vertex vertex{};

                tinyobj::index_t index = shape.mesh.indices[i];

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                m_model.min = glm::min(m_model.min, vertex.pos);
                m_model.max = glm::max(m_model.max, vertex.pos);

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };

                if (!uniqueVertices.contains(vertex))
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(mesh->vertices.size());
                    mesh->vertices.push_back(vertex);
                }

                mesh->indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    for (TriangleTree& tree : m_triangleTrees)
    {
        tree.branchTriangles.resize(maxDepth - 1);
    }

    for (uint32_t i = 0; i < m_model.meshes.size(); i++)
    {
        for (uint32_t j = 0; j < m_model.meshes[i].indices.size(); j += 3)
        {
            m_triangles.emplace_back(i, j);
            m_rootTriangles.emplace_back(m_triangles.size() - 1);
        }
    }
    m_triangles.shrink_to_fit();
}

Octree::Material Material::toOctreeMaterial() const
{
    Octree::Material mat{};
    mat.ambient = ambient;
    mat.diffuse = diffuse;
    mat.specular = specular;
    mat.specularComp = specularComp;
    return mat;
}

// This function is used to obtain material, normal and UV data for the provided Node
// The data is samples using the closest triangle intersect by the 6-connect test.
// It samples taking the baricentric coordinates of the intersection point.
void Voxelizer::sampleVoxel(NodeRef& node, uint8_t parallelIndex) const
{
    LeafNode leafNode{ 0 };
    TriangleLeafIndex closestLeaf{};
    closestLeaf.d = FLT_MAX;
    for (const TriangleLeafIndex& triangle : m_triangleTrees[parallelIndex].leafTriangles)
    {
        if (triangle.d < closestLeaf.d)
        {
            closestLeaf = triangle;
        }
    }
    // Baricentric at x = 1 - y - z
    glm::vec3 weights{1.0f - closestLeaf.baricentric.x - closestLeaf.baricentric.y, closestLeaf.baricentric.x, closestLeaf.baricentric.y};
    Triangle closestT = getTriangle(m_triangles[closestLeaf.index]);
    leafNode.setMaterial(getMaterialID(closestLeaf.index));
    leafNode.setUV(closestT.getWeightedUV(weights));
    leafNode.setNormal(closestT.getWeightedNormal(weights));
    auto [leaf1, leaf2] = leafNode.split();
    node.data1 = leaf1.toRaw();
    node.data2 = leaf2.toRaw();
}

// We want to get the smallest AABB that can contain the model
AABB Voxelizer::getModelAABB() const
{
    const float distX = std::abs(m_model.max.x - m_model.min.x);
    const float distY = std::abs(m_model.max.y - m_model.min.y);
    const float distZ = std::abs(m_model.max.z - m_model.min.z);
    const float size = std::max(distX, std::max(distY, distZ)) / 1.9f;
    return { (m_model.min + m_model.max) / 2.0f, size };
}

const std::vector<Material>& Voxelizer::getMaterials() const
{
    return m_model.materials;
}

std::string Voxelizer::getMaterialFilePath() const
{
    return m_baseDir;
}

std::array<glm::vec3, 3> Voxelizer::getTrianglePos(const uint32_t triangle) const
{
    const TriangleRootIndex rootIndex = m_triangles[triangle];

    return getTrianglePos(rootIndex);
}

Triangle Voxelizer::getTriangle(const uint32_t triangle) const
{
    const TriangleRootIndex rootIndex = m_triangles[triangle];
    return getTriangle(rootIndex);
}

Material Voxelizer::getMaterial(const uint32_t triangle) const
{
    return m_model.materials[getMaterialID(triangle)];
}

uint16_t Voxelizer::getMaterialID(const uint32_t triangle) const
{
    return m_triangles[triangle].meshIndex;
}

std::array<glm::vec3, 3> Voxelizer::getTrianglePos(const TriangleRootIndex rootIndex) const
{
    std::array<glm::vec3, 3> trianglePos{};
    const std::vector<Vertex>& vertices = m_model.meshes[rootIndex.meshIndex].vertices;
    const std::vector<uint32_t>& indices = m_model.meshes[rootIndex.meshIndex].indices;
    trianglePos[0] = vertices[indices[rootIndex.index]].pos;
    trianglePos[1] = vertices[indices[rootIndex.index + 1]].pos;
    trianglePos[2] = vertices[indices[rootIndex.index + 2]].pos;
    return trianglePos;
}

Triangle Voxelizer::getTriangle(TriangleRootIndex rootIndex) const
{
    Triangle trianglePos{};
    const std::vector<Vertex>& vertices = m_model.meshes[rootIndex.meshIndex].vertices;
    const std::vector<uint32_t>& indices = m_model.meshes[rootIndex.meshIndex].indices;
    trianglePos.v0 = vertices[indices[rootIndex.index]];
    trianglePos.v1 = vertices[indices[rootIndex.index + 1]];
    trianglePos.v2 = vertices[indices[rootIndex.index + 2]];
    return trianglePos;
}

Material Voxelizer::getMaterial(const TriangleRootIndex rootIndex) const
{
    return m_model.materials[rootIndex.meshIndex];
}


// TESTS

glm::vec3 axisGroup[] = {
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1}
};

// The 6-connect test is a test that will generate the smallest model without leaving holes
// It simply tests three rays along each axis against the triangles. Then checks that the intersected point (if any) is inside the AABB
// It is used for the leaves of the octree
// It stores the closest positive triangle for sampling
TriangleLeafIndex Voxelizer::AABBTriangle6Connect(const uint32_t index, const AABB shape) const
{
    TriangleLeafIndex current{shape.halfSize, {}, false};
    for (auto& axis : axisGroup)
    {
        float t;
        glm::vec2 bari;
        std::array<glm::vec3, 3> positions = getTrianglePos(index);
        if (glm::intersectRayTriangle(shape.center, axis, positions[0], positions[1], positions[2], bari, t))
        {
            if (std::abs(t) < current.d) 
                current = {std::abs(t), bari, true, index};
        }
    }
    return current;
}

static bool AABBTriangleSAT(const glm::vec3 v0, const glm::vec3 v1, const glm::vec3 v2, const float size, const glm::vec3 axis)
{
    const float p0 = glm::dot(v0, axis);
    const float p1 = glm::dot(v1, axis);
    const float p2 = glm::dot(v2, axis);

    const float r = glm::abs(glm::dot({ 1, 0, 0 }, axis)) + glm::abs(glm::dot({ 0, 1, 0 }, axis)) + glm::abs(glm::dot({ 0, 0, 1 }, axis));

    const float maxP = glm::max(p0, glm::max(p1, p2));
    const float minP = glm::min(p0, glm::min(p1, p2));

    return !(glm::max(-maxP, minP) > r * size);
}

// The separate axis theorem is used if a triangle is intersecting an AABB
// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/aabb-triangle.html
// This test is positive if any part of the triangle is inside the AABB
// It is used for the branches of the octree
bool Voxelizer::intersectAABBTriangleSAT(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, const AABB shape)
{
    v0 -= shape.center;
    v1 -= shape.center;
    v2 -= shape.center;

    glm::vec3 ab = glm::normalize(v1 - v0);
    glm::vec3 bc = glm::normalize(v2 - v1);
    glm::vec3 ca = glm::normalize(v0 - v2);

    //Cross ab, bc, and ca with (1, 0, 0)
    glm::vec3 a00(0.0f, -ab.z, ab.y);
    glm::vec3 a01(0.0f, -bc.z, bc.y);
    glm::vec3 a02(0.0f, -ca.z, ca.y);

    //Cross ab, bc, and ca with (0, 1, 0)
    glm::vec3 a10(ab.z, 0.0f, -ab.x);
    glm::vec3 a11(bc.z, 0.0f, -bc.x);
    glm::vec3 a12(ca.z, 0.0f, -ca.x);

    //Cross ab, bc, and ca with (0, 0, 1)
    glm::vec3 a20(-ab.y, ab.x, 0.0f);
    glm::vec3 a21(-bc.y, bc.x, 0.0f);
    glm::vec3 a22(-ca.y, ca.x, 0.0f);

    if (!AABBTriangleSAT(v0, v1, v2, shape.halfSize, a00) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a01) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a02) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a10) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a11) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a12) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a20) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a21) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, a22) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, { 1, 0, 0 }) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, { 0, 1, 0 }) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, { 0, 0, 1 }) ||
        !AABBTriangleSAT(v0, v1, v2, shape.halfSize, glm::cross(ab, bc)))
    {
        return false;
    }

    return true;
}

bool Voxelizer::intersectAABBPoint(const glm::vec3 point, const AABB shape)
{
    return glm::abs(point.x - shape.center.x) < shape.halfSize && glm::abs(point.y - shape.center.y) < shape.halfSize && glm::abs(point.z - shape.center.z) < shape.halfSize;
}

bool Voxelizer::doesAABBInteresect(const AABB& shape, const bool isLeaf, const uint8_t depth, const uint8_t parallelIndex)
{
    if (depth == 0) return true;

    const std::vector<uint32_t>& parentRef = depth - 1 == 0 ? m_rootTriangles : m_triangleTrees[parallelIndex].branchTriangles[depth - 2];
    if (!isLeaf) m_triangleTrees[parallelIndex].branchTriangles[depth - 1].clear();
    else m_triangleTrees[parallelIndex].leafTriangles.clear();

    for (const uint32_t triangle : parentRef)
    {
        std::array<glm::vec3, 3> tri = getTrianglePos(triangle);
        if (isLeaf)
        {
            // 6-connect test for leaves
            const TriangleLeafIndex result = AABBTriangle6Connect(triangle, shape);
            if (!result.hit) continue;
            // We store the positives into a vector for sampling
            m_triangleTrees[parallelIndex].leafTriangles.push_back(result);
        }
        else
        {
            // SAT test for branches
            if (!intersectAABBTriangleSAT(tri[0], tri[1], tri[2], shape)) continue;
            // We store the positives into a vector for the children to test. That way we avoid testing all triangles at all levels
            m_triangleTrees[parallelIndex].branchTriangles[depth - 1].push_back(triangle);
        }
    }
    if (isLeaf)
        return !m_triangleTrees[parallelIndex].leafTriangles.empty();
    return !m_triangleTrees[parallelIndex].branchTriangles[depth - 1].empty();
}

// VOXELIZATION GLOBAL FUNCTION

// We need a small function that wraps the parallel one since the octree asks for less arguments for the non parallel version
NodeRef Voxelizer::voxelize(const AABB& nodeShape, const uint8_t depth, const uint8_t maxDepth, void* data)
{
    return Voxelizer::parallelVoxelize(nodeShape, depth,maxDepth, data, 0);
}

NodeRef Voxelizer::parallelVoxelize(const AABB& nodeShape, const uint8_t depth, const uint8_t maxDepth, void* data, const uint8_t parallelIndex)
{
    Voxelizer& voxelizer = *static_cast<Voxelizer*>(data);
    NodeRef nodeRef{};
    nodeRef.isLeaf = depth >= maxDepth;
    nodeRef.exists = voxelizer.doesAABBInteresect(nodeShape, nodeRef.isLeaf, depth, parallelIndex);
    if (nodeRef.exists && nodeRef.isLeaf)
        voxelizer.sampleVoxel(nodeRef, parallelIndex);
    return nodeRef;
}

void Voxelizer::resetOctreeData(const uint8_t newDepth)
{
    for (TriangleTree& tree : m_triangleTrees)
    {
        tree.branchTriangles.clear();
        tree.branchTriangles.resize(newDepth - 1);
        tree.leafTriangles.clear();
    }
}
