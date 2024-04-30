#include "engine.hpp"
#include "utils/logger.hpp"

#include "Octree/octree.hpp"
#include "Octree/octree_generation.hpp"

#define LOAD true
#define SAVE true
constexpr uint8_t depth = 12;

int main()
{
#ifndef _DEBUG
    try {
    Logger::setLevels(Logger::INFO | Logger::WARN | Logger::ERR);
#else
    Logger::setLevels(Logger::ALL);
#endif

    Logger::setRootContext("Octree init");
#ifdef DEBUG_STRUCTURE
    Octree octree{depth, "assets/octree.bin"};
#else
	Octree octree{depth, "assets/octree.bin"};
#endif
    if constexpr (!LOAD)
    {
        Voxelizer voxelizer{"assets/sponza/sponza.obj", depth};
	    octree.generate(voxelizer.getModelAABB(), voxelize, &voxelizer);
        octree.setMaterialPath(voxelizer.getMaterialFilePath());
        for (const Material& mat : voxelizer.getMaterials())
            octree.addMaterial(mat.toOctreeMaterial(), mat.albedoMap, mat.normalMap);
        if constexpr (SAVE)
            octree.dump("assets/octree.bin");
	}
    else
    {
        octree.load();
    }
    octree.packAndFinish();

	Engine engine{static_cast<uint32_t>(octree.getMaterialTextures().size())};

    Logger::setRootContext("Engine context init");
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
