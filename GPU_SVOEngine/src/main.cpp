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
    //octree.preallocate(4000LL * 1024 * 1024);
	octree.populateSample(11);

	engine.configureOctreeBuffer(octree, 200.0f);
    octree.clear();
	engine.run();
#ifndef _DEBUG
    }
    catch (const std::exception& e) {
        Logger::print(std::string("[RUNTIME ERROR] ") + e.what(), Logger::LevelBits::ERR);
        return EXIT_FAILURE;
    }
#endif
	return EXIT_SUCCESS;
}
