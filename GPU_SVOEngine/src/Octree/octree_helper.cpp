#include "octree_helper.hpp"


NearPtr::NearPtr(const uint16_t ptr, const bool isFar)
    : addr(ptr & 0x7FFF), farFlag(isFar ? 1 : 0)
{

}

uint16_t NearPtr::getPtr() const
{
    return addr;
}

bool NearPtr::isFar() const
{
    return farFlag == 1;
}

uint16_t NearPtr::toRaw() const
{
    return isFar() ? addr | 0x8000 : addr & 0x7FFF;
}

void NearPtr::setPtr(const uint16_t ptr)
{
    addr = ptr & 0x7FFF;
}

void NearPtr::setFar(const bool isFar)
{
    farFlag = isFar ? 1 : 0;
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

uint8_t BitField::toRaw() const
{
    return field;
}

bool BitField::operator==(const BitField& other) const
{
    return field == other.field;
}