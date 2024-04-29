#include "engine.hpp"
#include "utils/logger.hpp"

#include "Octree/octree.hpp"
#include "Octree/octree_generation.hpp"

#define LOAD true
constexpr uint8_t depth = 12;

int main()
{
#ifndef _DEBUG
    try {
    Logger::setLevels(Logger::INFO | Logger::WARN | Logger::ERR);
#else
    Logger::setLevels(Logger::ALL);
#endif

	Engine engine{};
#ifdef DEBUG_STRUCTURE
    Octree octree{depth, "assets/octree.bin"};
#else
	Octree octree{depth, "assets/octree.bin"};
#endif
    if constexpr (!LOAD)
    {
        Voxelizer voxelizer{"assets/San_Miguel/san-miguel.obj", depth};
	    octree.generate(voxelizer.getModelAABB(), voxelize, &voxelizer);
	}

#ifdef DEBUG_STRUCTURE
    Logger::print(octree.toString(), Logger::LevelBits::INFO);
#endif

    if constexpr (LOAD) 
        octree.load();

	engine.configureOctreeBuffer(octree, 7 * depth);
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
