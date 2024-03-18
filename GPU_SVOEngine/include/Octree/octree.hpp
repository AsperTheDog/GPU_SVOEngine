#pragma once
#include <cstdint>
#include <vector>

// Helper classes

class NearPtr
{
public:
	explicit NearPtr(uint16_t ptr);

	[[nodiscard]] uint16_t getPtr() const;
	[[nodiscard]] bool isFar() const;

private:
	uint16_t raw;
};

class BitField
{
public:
	explicit BitField(uint8_t field);

	[[nodiscard]] bool getBit(uint8_t index) const;
	void setBit(uint8_t index, bool value);

private:
	uint8_t field;
};

// Octree classes

struct OctreeNode
{
	NearPtr ptr;
	BitField childMask;
	BitField leafMask;
};

class Octree
{
	std::vector<OctreeNode> data;
};

