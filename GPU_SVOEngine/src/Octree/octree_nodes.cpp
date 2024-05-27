#include "octree_nodes.hpp"


BranchNode::BranchNode(const uint32_t raw)
{
    const bool farFlag =     (raw & 0x80000000) >> 31;
    const uint16_t address = (raw & 0x7FFF0000) >> 16;
    childMask =     BitField((raw & 0x0000FF00) >> 8);
    leafMask =       BitField(raw & 0x000000FF);
    ptr = NearPtr(address, farFlag);
}

uint32_t BranchNode::toRaw() const
{
    return ptr.toRaw() << 16 | childMask.toRaw() << 8 | leafMask.toRaw();
}

LeafNode::LeafNode(const uint64_t raw)
{
    uvx =      (raw & 0xFFF0000000000000) >> 52;
    uvy =      (raw & 0x000FFF0000000000) >> 40;
    material = (raw & 0x000000FFC0000000) >> 30;
    normalx =  (raw & 0x000000003FF00000) >> 20;
    normaly =  (raw & 0x00000000000FFC00) >> 10;
    normalz =   raw & 0x00000000000003FF;
}

void LeafNode::setUV(glm::vec2 uv)
{
    // clamp uv to [0, 1]
    while (uv.x < 0.f) uv.x += 1.f;
    while (uv.y < 0.f) uv.y += 1.f;
    while (uv.x > 1.f) uv.x -= 1.f;
    while (uv.y > 1.f) uv.y -= 1.f;

    constexpr uint16_t max = 0xFFF;
    uvx = std::min(static_cast<uint16_t>(uv.x * max), max);
    uvy = std::min(static_cast<uint16_t>(uv.y * max), max);
}

void LeafNode::setNormal(const glm::vec3& normal)
{
    constexpr float max = 0x3FF;
    constexpr uint16_t half_max = 0x200;
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normaly = static_cast<uint16_t>(std::min(normal.y * half_max + half_max, max));
    normalz = static_cast<uint16_t>(std::min(normal.z * half_max + half_max, max));
}

void LeafNode::setMaterial(const uint16_t mat)
{
    material = mat & 0x3FF;
}

glm::vec2 LeafNode::getUV() const
{
    return { static_cast<float>(uvx) / 0xFFF, static_cast<float>(uvy) / 0xFFF };
}

glm::vec3 LeafNode::getNormal() const
{
    return { static_cast<float>(normalx) / 0x03FF, static_cast<float>(normaly) / 0x03FF, static_cast<float>(normalz) / 0x03FF };
}

uint16_t LeafNode::getMaterial(const LeafNode1 other) const
{
    return static_cast<uint16_t>((material & 0x003) << 8 | (other.material & 0xFF));
}

uint64_t LeafNode::toRaw() const
{
    uint64_t raw = 0;
    raw |= static_cast<uint64_t>(uvx) << 52;
    raw |= static_cast<uint64_t>(uvy) << 40;
    raw |= static_cast<uint64_t>(material) << 30;
    raw |= static_cast<uint64_t>(normalx) << 20;
    raw |= static_cast<uint64_t>(normaly) << 10;
    raw |= static_cast<uint64_t>(normalz);
    return raw;
}

std::pair<LeafNode1, LeafNode2> LeafNode::split() const
{
    const uint64_t raw = toRaw();
    const uint32_t raw1 = static_cast<uint32_t>((raw & 0xFFFFFFFF00000000) >> 32);
    const uint32_t raw2 =  static_cast<uint32_t>(raw & 0x00000000FFFFFFFF);
    return {LeafNode1{raw1}, LeafNode2{raw2}};
}

LeafNode LeafNode::combine(const LeafNode1 leafNode1, const LeafNode2 leafNode2)
{
    uint64_t raw = 0;
    raw |= leafNode2.toRaw();
    raw |= static_cast<uint64_t>(leafNode1.toRaw()) << 32;
    return LeafNode{raw};
}

LeafNode1::LeafNode1(const uint32_t raw)
{
    uvx =      (raw & 0xFFF00000) >> 20;
    uvy =      (raw & 0x000FFF00) >> 8;
    material = (raw & 0x000000FF);
}

void LeafNode1::setUV(const glm::vec2& uv)
{
    constexpr uint16_t max = 0xFFF;
    uvx = std::min(static_cast<uint16_t>(uv.x * max), max);
    uvy = std::min(static_cast<uint16_t>(uv.y * max), max);
}

void LeafNode1::set(const uint16_t mat)
{
    material = mat & 0x0FF;
}

glm::vec2 LeafNode1::getUV() const
{
    return { static_cast<float>(uvx) / 0xFFF, static_cast<float>(uvy) / 0xFFF };
}

uint16_t LeafNode1::getMaterial(const LeafNode2 other) const
{
    return static_cast<uint16_t>((other.material &  0x003) << 8 | (material & 0x0FF));
}

uint32_t LeafNode1::toRaw() const
{
    return uvx << 20 | uvy << 8 | material;
}

LeafNode2::LeafNode2(const uint32_t raw)
{
    material = (raw & 0xC0000000) >> 30;
    normalx =  (raw & 0x3FF00000) >> 20;
    normaly =  (raw & 0x000FFC00) >> 10;
    normalz =   raw & 0x000003FF;
}

void LeafNode2::setNormal(const glm::vec3& normal)
{
    constexpr float max = 0x3FF;
    constexpr uint16_t half_max = 0x200;
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normalx = static_cast<uint16_t>(std::min(normal.x * half_max + half_max, max));
    normaly = static_cast<uint16_t>(std::min(normal.y * half_max + half_max, max));
    normalz = static_cast<uint16_t>(std::min(normal.z * half_max + half_max, max));
}

void LeafNode2::setMaterial(const uint16_t mat)
{
    this->material = mat & 0x0300;
}

glm::vec3 LeafNode2::getNormal() const
{
    constexpr uint16_t half_max = 0x1FF;
    return {
        (static_cast<float>(normalx) / half_max - 1.f),
        (static_cast<float>(normaly) / half_max - 1.f),
        (static_cast<float>(normalz) / half_max - 1.f)
    };
}

uint16_t LeafNode2::getMaterial(const LeafNode1 other) const
{
    return static_cast<uint16_t>((material & 0x0003) << 8 | (other.material & 0x00FF));
}

uint32_t LeafNode2::toRaw() const
{
    return (material & 0x0003) << 30 | normalx << 20 | normaly << 10 | normalz;
}

FarNode::FarNode(const uint32_t raw)
{
    ptr = raw;
}

uint32_t FarNode::toRaw() const
{
    return ptr;
}
