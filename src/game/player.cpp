#include "player.h"

Player::Player() : position{glm::vec3(0.0f)}, rotation{glm::quat(1.0f, 0.0f, 0.0f, 0.0f)}, scale{glm::vec3(1.0f)}, health{100}, max_health(100), speed{10} {}

void Player::take_damage(float damage) {
    health -= damage;
    if (health < 0.0f) {
        health = 0.0f;
    }
}

bool Player::is_alive() const {
    return health > 0.0f;
}