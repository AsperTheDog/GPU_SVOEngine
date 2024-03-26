#include "engine.hpp"
#include "logger.hpp"
#include "Octree/octree.hpp"

int main()
{
#ifndef _DEBUG
    try {
#endif
	Engine engine{};

	Octree octree;
    octree.preallocate(400LL * 1024 * 1024);
	octree.populateSample(11);

	engine.configureOctreeBuffer(octree, 200.0f);
	engine.run();
#ifndef _DEBUG
    }
    catch (const std::exception& e) {
        Logger::print(std::string("[RUNTIME ERROR] ") + e.what());
        return EXIT_FAILURE;
    }
#endif
	return EXIT_SUCCESS;
}
