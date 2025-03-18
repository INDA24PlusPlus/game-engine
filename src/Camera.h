#ifndef _CAMERA_H
#define _CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    constexpr glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 pos;
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;

    f32 yaw;
    f32 pitch;
    f32 speed;
    f32 fov;

    glm::mat4 view_matrix() {
        return glm::lookAt(pos, pos + forward, up);
    }

    enum class Direction {
        forward,
        backward,
        left,
        right,
    };

    void move(Direction dir, f32 delta_time) {
    }

    void update_vectors() {
        forward.x = glm::cos(yaw) * glm::cos(pitch);
        forward.y = glm::sin(pitch);
        forward.z = glm::sin(yaw) * glm::cos(pitch);
        forward = glm::normalize(forward);
        right = glm::normalize(glm::cross(forward, world_up));
        up = glm::normalize(glm::cross(right, forward));
    }
};

#endif
