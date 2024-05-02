#pragma once
#include <cstdint>

class NearPtr
{
public:
    explicit NearPtr(uint16_t ptr, bool isFar);

    [[nodiscard]] uint16_t getPtr() const;
    [[nodiscard]] bool isFar() const;

    [[nodiscard]] uint16_t toRaw() const;

    void setPtr(uint16_t ptr);
    void setFar(bool isFar);

private:
    uint16_t addr : 15;
    uint16_t farFlag : 1;
};

class BitField
{
public:
    explicit BitField(uint8_t field);

    [[nodiscard]] bool getBit(uint8_t index) const;
    void setBit(uint8_t index, bool value);

    [[nodiscard]] uint8_t toRaw() const;

    bool operator==(const BitField& other) const;

private:
    uint8_t field;
};