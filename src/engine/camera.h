#ifndef _CAMERA_H
#define _CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine {

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

    inline glm::mat4 view_matrix() {
        return glm::lookAt(pos, pos + forward, up);
    }

    enum class Direction {
        forward,
        backward,
        left,
        right,
    };

    void move(Direction dir, f32 delta_time);
    void update_vectors();
};

}

#endif
