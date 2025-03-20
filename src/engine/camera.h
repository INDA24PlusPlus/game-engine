#ifndef _CAMERA_H
#define _CAMERA_H

#include "core.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine {

struct Camera {
    static constexpr glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 pos;
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;

    f32 yaw;
    f32 pitch;
    f32 speed;
    f32 fov;

    static constexpr Camera init(glm::vec3 pos, f32 speed, f32 fov) {
        Camera cam = {
            .pos = pos,
            .forward = glm::vec3(0.0f, 0.0f, -1.0f),
            .yaw = glm::radians(-90.0f),
            .speed = speed,
            .fov = fov,
        };
        cam.update_vectors();
        return cam;
    }

    inline glm::mat4 view_matrix() {
        return glm::lookAt(pos, pos + forward, up);
    }

    enum class Direction {
        forward,
        backward,
        left,
        right,
        upward,
        downward,
    };

    void move(Direction dir, f32 delta_time) {
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
            case Direction::upward: {
                pos += up * vel;
                break;
            }
            case Direction::downward: {
                pos -= up * vel;
                break;
            }
        }
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

}

#endif
