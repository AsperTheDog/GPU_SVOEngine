#include "engine.hpp"
#include "utils/logger.hpp"

#include "Octree/octree.hpp"

struct RandomData
{
    float density = 0.0f;
    std::vector<glm::vec3> color{};
};

glm::vec3 shiftColor(const glm::vec3 color)
{
    float r = std::min(std::max(color.r + (rand() % 2 == 0 ? 0.4f : -0.4f), 0.0f), 15.0f);
    float g = std::min(std::max(color.g + (rand() % 2 == 0 ? 0.4f : -0.4f), 0.0f), 15.0f);
    float b = std::min(std::max(color.b + (rand() % 2 == 0 ? 0.4f : -0.4f), 0.0f), 15.0f);
    return { r, g, b };
}

NodeRef generateRandomly(const AABB& nodeShape, const uint8_t depth, const uint8_t maxDepth, void* data)
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
        LeafNode leafNode{0};
        leafNode.setColor(randomData.color[depth]);
        leafNode.setNormal(glm::vec3{0.0f, 1.0f, 0.0f});
        nodeRef.data = leafNode.toRaw();
        nodeRef.isLeaf = true;
    }

    return nodeRef;
}

int main()
{
#ifndef _DEBUG
    try {
    Logger::setLevels(Logger::WARN | Logger::ERR);
#else
    Logger::setLevels(Logger::ALL);
#endif
    constexpr uint8_t depth = 3;

	Engine engine{};
	Octree octree{depth};
    RandomData randomData{1.0f};
    randomData.color.resize(depth + 1);
    randomData.color[0] = glm::vec3{8.0f, 3.0f, 8.0f};
	octree.generate(generateRandomly, &randomData);

#ifdef DEBUG_STRUCTURE
    Logger::print(octree.toString(), Logger::LevelBits::DEBUG);
#endif

	engine.configureOctreeBuffer(octree, 2 * depth);
    octree.clear();
	engine.run();
#ifndef _DEBUG
    }
    catch (const std::exception& e) {
        Logger::print(e.what(), Logger::LevelBits::ERR);
        return EXIT_FAILURE;
    }
#endif
	return EXIT_SUCCESS;
}
