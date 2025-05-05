#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct Player {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    float health;
    float max_health;
    float speed;

    Player();

    void take_damage(float damage);
    bool is_alive() const;
};