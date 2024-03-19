#include "camera.hpp"

#include <glm/gtx/transform.hpp>

Camera::Camera(const glm::vec3 pos, const glm::vec3 dir, const float fov, const float near, const float far)
	: m_position(pos), m_front(dir), m_fov(fov), m_near(near), m_far(far), m_viewDirty(true), m_projDirty(true), m_invPVDirty(true)
{

}

void Camera::move(const glm::vec3 dir)
{
	m_position += dir;
}

void Camera::lookAt(const glm::vec3 target)
{
	m_front = glm::normalize(target - m_position);
}

void Camera::setPosition(const glm::vec3 pos)
{
	m_position = pos;
}

void Camera::setDir(const glm::vec3 dir)
{
	m_front = dir;
}

void Camera::setScreenSize(const uint32_t width, const uint32_t height)
{
	m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	m_projDirty = true;
}

void Camera::setProjectionData(float fov, float near, float far)
{
	m_fov = fov;
	m_near = near;
	m_far = far;
	m_projDirty = true;
}

glm::vec3 Camera::getPosition() const
{
	return m_position;
}

glm::vec4 Camera::getPositionV4() const
{
	return {m_position, 0.0f};
}

glm::vec3 Camera::getDir() const
{
	return m_front;
}

glm::mat4& Camera::getViewMatrix()
{
	if (m_viewDirty)
	{
		m_viewMatrix = glm::lookAt(m_position, m_position + m_front, glm::vec3(0, 1, 0));
		m_viewDirty = false;
		m_invPVDirty = true;
	}

	return m_viewMatrix;
}

glm::mat4& Camera::getProjMatrix()
{
	if (m_projDirty)
	{
		m_projMatrix = glm::perspective(glm::radians(m_fov), 16.0f/9, m_near, m_far);
		m_projDirty = false;
		m_invPVDirty = true;
	}

	return m_projMatrix;
}

glm::mat4& Camera::getInvPVMatrix()
{
	if (m_invPVDirty)
	{
		m_invPVMatrix = glm::inverse(getProjMatrix() * getViewMatrix());
		m_invPVDirty = false;
	}

	return m_invPVMatrix;
}

Camera::Data Camera::getData()
{
	return {getPositionV4(), getInvPVMatrix()};
}
