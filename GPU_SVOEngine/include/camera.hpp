#pragma once
#include <glm/glm.hpp>

class Camera
{
public:
	struct Data
	{
		glm::vec4 position;
		glm::mat4 invPVMatrix;
	};

	Camera(glm::vec3 pos, glm::vec3 dir, float fov = 90.0f, float near = 0.1f, float far = 100.0f);

	void move(glm::vec3 dir);
	void lookAt(glm::vec3 target);

	void setPosition(glm::vec3 pos);
	void setDir(glm::vec3 dir);

	void setScreenSize(uint32_t width, uint32_t height);
	void setProjectionData(float fov, float near, float far);

	[[nodiscard]] glm::vec3 getPosition() const;
		[[nodiscard]] glm::vec4 getPositionV4() const;
	[[nodiscard]] glm::vec3 getDir() const;

	glm::mat4& getViewMatrix();
	glm::mat4& getProjMatrix();
	glm::mat4& getInvPVMatrix();

	[[nodiscard]] Data getData();

private:
	glm::vec3 m_position;
	glm::vec3 m_front;
	float m_fov;
	float m_aspectRatio;
	float m_near;
	float m_far;

	bool m_viewDirty;
	glm::mat4 m_viewMatrix{};
	bool m_projDirty;
	glm::mat4 m_projMatrix{};
	bool m_invPVDirty;
	glm::mat4 m_invPVMatrix{};
};

