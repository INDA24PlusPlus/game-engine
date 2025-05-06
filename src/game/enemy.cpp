#include "enemy.h"
#include "state.h"

Enemy::Enemy() : position{glm::vec3(20, 0, 20)}, rotation{glm::quat(1.0f, 0.0f, 0.0f, 0.0f)}, scale{glm::vec3(1.0f)}, health{100}, speed{3}, hover_time(0), cooldown{0} {}

void Enemy::update(State &state) {
    if (cooldown > 0) {
        cooldown -= state.delta_time;
        return;
    }
    glm::vec3 enemy_target_look = state.player.position - position;
    enemy_target_look.y = 0;

    float player_distance = glm::length(enemy_target_look);

    if (player_distance > 2) {
        // Rotate
        enemy_target_look = glm::normalize(enemy_target_look);
        glm::quat target_rotation = glm::quatLookAt(enemy_target_look, glm::vec3(0, 1, 0));
        rotation = glm::slerp(rotation, target_rotation, state.delta_time * 2.0f);
        
        // Move in facing direction
        glm::vec3 forward = rotation * glm::vec3(0, 0, -1);
        position += forward * speed * state.delta_time;

        // hover effect sine function
        position.y = glm::sin(hover_time) * 0.3;
        hover_time += state.delta_time * speed;
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