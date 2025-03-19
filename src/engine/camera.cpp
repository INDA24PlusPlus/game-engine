#include "Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine {

void Camera::move(Direction dir, f32 delta_time) {
    auto vel = speed * delta_time;
    switch (dir) {
        case Direction::forward: {
            pos += forward * vel;
            break;
        }
        case Direction::backward: {
            pos -= forward * vel;
            break;
        }
        case Direction::right: {
            pos += right * vel;
            break;
        }
        case Direction::left: {
            pos -= right * vel;
            break;
        }
    }
}

void Camera::update_vectors() {
    forward.x = glm::cos(yaw) * glm::cos(pitch);
    forward.y = glm::sin(pitch);
    forward.z = glm::sin(yaw) * glm::cos(pitch);
    forward = glm::normalize(forward);
    right = glm::normalize(glm::cross(forward, world_up));
    up = glm::normalize(glm::cross(right, forward));
}
}

#endif
