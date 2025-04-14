#ifndef _CAMERA_H
#define _CAMERA_H

#include "core.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine {


class Camera {
public:
    glm::vec3 m_pos;
    glm::quat m_orientation;
    f32 m_speed;

    void init(glm::vec3 pos, f32 speed);
    void rotate(f32 yaw, f32 pitch);
    void move(glm::vec3 dir, f32 delta_time);
    glm::mat4 get_view_matrix() const;
};

}

#endif
