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
	Octree octree{depth, "assets/octree.bin"};
    //RandomData randomData{0.6f};
    //randomData.color.resize(depth + 1);
    //randomData.color[0] = glm::vec3{8.0f, 3.0f, 8.0f};
	//octree.generate(generateRandomly, &randomData);
    octree.load();

//#ifdef DEBUG_STRUCTURE
//    Logger::print(octree.toString(), Logger::LevelBits::DEBUG);
//#endif

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
