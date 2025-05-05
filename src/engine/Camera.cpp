#include "Camera.h"
#include "glm/fwd.hpp"
#include "glm/gtc/quaternion.hpp"

#include <iostream>

namespace engine {

void Camera::init(glm::vec3 pos, f32 speed) {
    m_pos = pos;
    m_speed = speed;
    m_orientation = glm::quat(1, 0, 0, 0);
}

void Camera::rotate(f32 yaw, f32 pitch) {
    m_pitch += pitch;
    m_yaw += yaw;
    m_pitch = glm::clamp(m_pitch, glm::radians(-89.0f), glm::radians(89.0f));

    auto q_yaw = glm::angleAxis(m_yaw, glm::vec3(0, 1, 0));
    auto q_pitch = glm::angleAxis(m_pitch, glm::vec3(1, 0, 0));

    m_orientation = q_yaw * q_pitch;
    m_orientation = glm::normalize(m_orientation);
}

void Camera::move(glm::vec3 dir, f32 delta_time) {
    auto forward = m_orientation * glm::vec3(0, 0, -1);
    auto right = m_orientation * glm::vec3(1, 0, 0);
    auto movement = (right * dir.x + forward * dir.z) * m_speed * delta_time;
    m_pos += movement;
}

glm::mat4 Camera::get_view_matrix() const {
    auto rotation = glm::mat4_cast(glm::conjugate(m_orientation));
    auto translation = glm::translate(glm::mat4(1.0f), -m_pos);
    return rotation * translation;
}

}