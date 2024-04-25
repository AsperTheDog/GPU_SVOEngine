#include "engine.hpp"
#include "utils/logger.hpp"

#include "Octree/octree.hpp"

int main()
{
#ifndef _DEBUG
    try {
    Logger::setLevels(Logger::WARN | Logger::ERR);
#else
    Logger::setLevels(Logger::ALL);
#endif
	Engine engine{};

	Octree octree;
    constexpr uint8_t depth = 12;
	octree.generateRandomly(depth, 0.6f);

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
