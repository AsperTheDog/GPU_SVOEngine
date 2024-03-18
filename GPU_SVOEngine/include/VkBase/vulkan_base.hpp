#pragma once

#include <cstdint>

class VulkanBase
{
public:
	[[nodiscard]] uint32_t getID() const { return m_id; }

protected:
	VulkanBase() : m_id(s_idCounter++) {}

	uint32_t m_id = 0;

private:
	inline static uint32_t s_idCounter = 0;
};
