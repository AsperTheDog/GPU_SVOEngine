#include "engine.hpp"
#include "utils/logger.hpp"

#include "Octree/octree.hpp"
#include "Octree/octree_generation.hpp"

int main()
{
#ifndef _DEBUG
    try {
    Logger::setLevels(Logger::INFO | Logger::WARN | Logger::ERR);
#else
    Logger::setLevels(Logger::ALL);
#endif
    constexpr uint8_t depth = 11;

	Engine engine{};
#ifdef DEBUG_STRUCTURE
    Octree octree{depth, "assets/octreed.bin"};
#else
	Octree octree{depth, "assets/octree.bin"};
#endif

    //RandomData randomData{1.0f};
    //randomData.color.resize(depth + 1);
    //randomData.color[0] = glm::vec3{8.0f, 3.0f, 8.0f};
    //octree.generate({{0.0f, 0.0f, 0.0f}, 1.0f}, generateRandomly, &randomData);
    Voxelizer voxelizer{"assets/test_cube.obj", depth};
	octree.generate(voxelizer.getModelAABB(), voxelize, &voxelizer);

#ifdef DEBUG_STRUCTURE
    Logger::print(octree.toString(), Logger::LevelBits::INFO);
#endif

    //octree.load();

	engine.configureOctreeBuffer(octree, 2 * depth);
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
