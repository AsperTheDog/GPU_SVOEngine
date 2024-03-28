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

    [[nodiscard]] uint16_t toRaw() const;

private:
	uint16_t raw;
};

class BitField
{
public:
	explicit BitField(uint8_t field);

	[[nodiscard]] bool getBit(uint8_t index) const;
	void setBit(uint8_t index, bool value);

    [[nodiscard]] uint8_t toRaw() const;

private:
	uint8_t field;
};

// Octree classes

struct BranchNode {
    explicit BranchNode(uint32_t raw);
    
	NearPtr ptr{0, false};
	BitField leafMask{0};
	BitField childMask{0};

    [[nodiscard]] uint32_t toRaw() const;
};

struct LeafNode {
    explicit LeafNode(uint32_t raw);

	glm::u8vec3 color{};
	glm::u8vec3 normal{};
	uint8_t material;

    [[nodiscard]] uint32_t toRaw() const;
};

struct FarNode {
	uint32_t ptr;
};

class Octree
{
public:
	[[nodiscard]] size_t getSize() const;
	[[nodiscard]] size_t getByteSize() const;
	[[nodiscard]] uint32_t& operator[](size_t index);
    [[nodiscard]] uint32_t getFarPtr(size_t index) const;

    [[nodiscard]] uint8_t getDepth() const;

    void preallocate(size_t size);
    void populateSample(uint8_t maxDepth);
    void pack();
	void addChild(uint32_t child);
	void addChild(BranchNode child);
	void addChild(LeafNode child);
	void addChild(FarNode child);
    NearPtr pushFarPtr(size_t childPos);

	void* getData();

    void clear();

private:
    uint32_t& get(size_t index);
    bool populateRec(uint64_t parentPos, uint8_t currentDepth, uint8_t maxDepth, glm::vec3 color);

	std::vector<uint32_t> data;
    std::vector<uint32_t> farPtrs;
	uint8_t depth = 0;
};

