#include "engine.hpp"
#include "Octree/octree.hpp"

int main()
{
	Engine engine{};

	Octree octree;
	octree.populateSample(3);

	engine.run();
	return 0;
}
