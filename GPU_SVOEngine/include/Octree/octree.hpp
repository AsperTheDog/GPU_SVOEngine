#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

// Helper classes

class NearPtr
{
public:
	explicit NearPtr(uint16_t ptr, bool isFar);

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

union OctreeNode
{
	OctreeNode();

	struct {
		NearPtr ptr;
		BitField childMask{0};
		BitField leafMask{0};
	} branch;

	struct {
		uint8_t material;
		glm::u8vec3 color;
	} leaf;

	struct {
		uint32_t ptr;
	} far;
};

class Octree
{
public:
	[[nodiscard]] size_t getSize() const;
	[[nodiscard]] size_t getByteSize() const;
	[[nodiscard]] OctreeNode& operator[](size_t index);
	[[nodiscard]] const OctreeNode& operator[](size_t index) const;

	void populateSample(uint8_t depth);
	void addChild(OctreeNode child);

	void* getData();

private:
	std::vector<OctreeNode> data;
	uint8_t depth = 0;
};

