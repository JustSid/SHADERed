#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Camera.h"

namespace ed
{
	class ArcBallCamera : public Camera
	{
	public:
		const static float MaxDistance;
		const static float MinDistance;
		const static float MaxRotationY;

	public:
		ArcBallCamera() { Reset(); }

		virtual void Reset();

		void SetDistance(float d);
		void Move(float d);
		void Yaw(float rx);
		void Pitch(float ry);
		void Roll(float rz);
		void SetYaw(float r);
		void SetPitch(float r);
		void SetRoll(float r);

		inline float GetDistance() { return m_distance; }
		inline virtual glm::vec3 GetRotation() { return glm::vec3(m_pitch, m_yaw, m_roll); }

		virtual glm::vec4 GetPosition();
		virtual glm::vec4 GetUpVector();

		virtual glm::mat4 GetMatrix();

	private:
		float m_distance;
		float m_yaw, m_pitch, m_roll;
	};
}