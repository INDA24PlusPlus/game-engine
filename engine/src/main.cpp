#include "ecs.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

struct Position : public Component<Position> {
    float x;
    float y;
};

struct Velocity : public Component<Velocity> {
    float x;
    float y;
};

struct Iteration : public Component<Iteration> {
    float value;
};

class MovementSystem : public System<MovementSystem> {
    public:
        void update(ECS& ecs) {
            for (auto& entity : entities) {
                Position& position = ecs.get_component<Position>(entity);
                Velocity& velocity = ecs.get_component<Velocity>(entity);
                position.x += velocity.x;
                position.y += velocity.y;
            }
        }
};

class VaryVelocitySystem : public System<VaryVelocitySystem> {
    public:
        void update(ECS& ecs) {
            for (auto& entity : entities) {
                Velocity& velocity = ecs.get_component<Velocity>(entity);
                Iteration& iteration = ecs.get_component<Iteration>(entity);
                velocity.x = sin(iteration.value);
                velocity.y = cos(iteration.value);
                iteration.value += 0.1f;
            }
        }
};

void run() {
    ECS ecs = ECS();
    ecs.register_component<Position>();
    ecs.register_component<Velocity>();
    ecs.register_component<Iteration>();

    MovementSystem *movement_system = ecs.register_system<MovementSystem>();
    VaryVelocitySystem *vary_velocity_system = ecs.register_system<VaryVelocitySystem>();

    Signature movement_signature = 0;
    movement_signature = set_signature(movement_signature, Position::get_id());
    movement_signature = set_signature(movement_signature, Velocity::get_id());
    ecs.set_system_signature<MovementSystem>(movement_signature);

    Signature vary_velocity_signature = 0;
    vary_velocity_signature = set_signature(vary_velocity_signature, Velocity::get_id());
    vary_velocity_signature = set_signature(vary_velocity_signature, Iteration::get_id());
    ecs.set_system_signature<VaryVelocitySystem>(vary_velocity_signature);

    Entity entity = ecs.create_entity();

    Position position = Position();
    position.x = 1.0f;
    position.y = 2.0f;

    Velocity velocity = Velocity();
    velocity.x = 1.0f;
    velocity.y = 1.0f;

    Iteration iteration = Iteration();
    iteration.value = 0.0f;

    ecs.add_component(entity, position);
    ecs.add_component(entity, velocity);
    ecs.add_component(entity, iteration);

    while (true) {
        movement_system->update(ecs);
        vary_velocity_system->update(ecs);
        Position& new_position = ecs.get_component<Position>(entity);
        printf("Position: %f %f\n", new_position.x, new_position.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int main() {
    run();
    return 0;
}
