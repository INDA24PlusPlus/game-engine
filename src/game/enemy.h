#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct State;

struct Enemy {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    float health;
    float speed;
    float hover_time;

    float cooldown;

    Enemy();

    void update(State &state);
    void take_damage(float damage);
    bool is_alive() const;
};