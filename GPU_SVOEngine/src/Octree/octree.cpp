#include "Octree/octree.hpp"

NearPtr::NearPtr(const uint16_t ptr) : raw(ptr)
{
}

uint16_t NearPtr::getPtr() const
{
	// Set most significant bit to 0
	return raw & 0x7FFF;
}

bool NearPtr::isFar() const
{
	// Check most significant bit
	return (raw & 0x8000) != 0;
}

BitField::BitField(const uint8_t field) : field(field)
{
}

bool BitField::getBit(const uint8_t index) const
{
	// Check if bit is set
	return (field & (1 << index)) != 0;
}

void BitField::setBit(const uint8_t index, const bool value)
{
	// Set or clear bit
	if (value)
	{
		field |= (1 << index);
	}
	else
	{
		field &= ~(1 << index);
	}
}
