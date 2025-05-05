#include "enemy.h"
#include "state.h"

Enemy::Enemy() : position{glm::vec3(20, 0, 20)}, rotation{glm::quat(1.0f, 0.0f, 0.0f, 0.0f)}, scale{glm::vec3(1.0f)}, health{100}, speed{3}, cooldown{0} {}

void Enemy::update(State &state) {
    glm::vec3 enemy_move = state.player.position - position;
    enemy_move.y = 0;

    float player_distance = glm::length(enemy_move);

    if (player_distance > 2) {
        // Move
        if (player_distance != 0) enemy_move = glm::normalize(enemy_move);
        
        glm::quat target_rotation = glm::quatLookAt(enemy_move, glm::vec3(0, 1, 0));
        position += enemy_move * speed * state.delta_time;
        rotation =
            glm::slerp(rotation, target_rotation, state.delta_time * 8.0f);
    } else {
        // Damage player
        state.player.take_damage(10);
        cooldown = 0.5;
    }
}

void Enemy::take_damage(float damage) {
    health -= damage;
    if (health < 0.0f) {
        health = 0.0f;
    }
}

bool Enemy::is_alive() const {
    return health > 0.0f;
}