#include "engine/ecs/component.hpp"
#include "engine/ecs/system.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/utils/logging.h"
#include <cmath>
#include <cstdio>
class CPlayer : public Component<CPlayer> {
public:
};

class CVelocity : public Component<CVelocity> {
public:
    float x, y;
};

class CPosition : public Component<CPosition> {
public:
    float x, y;
};

class CEnemy : public Component<CEnemy> {
    public:
};

class CSize : public Component<CSize> {
    public:
        float width, height;
};

class CName : public Component<CName> {
    public:
        char name[32];
};

class SMove : public System<SMove> {
    public:
        SMove() {
            queries[0] = Query(CPosition::get_id(), CVelocity::get_id());
            query_count = 1;
        }

        void update(ECS &ecs) {
            auto entities = get_query(0)->get_entities();
            Iterator it = { .next = 0 };
            Entity e;
            while (entities->next(it, e)) {
                CPosition& pos = ecs.get_component<CPosition>(e);
                CVelocity& vel = ecs.get_component<CVelocity>(e);
                pos.x += vel.x;
                pos.y += vel.y;
            }
        }
};

class SWalkTowardsPlayer : public System<SWalkTowardsPlayer> {
    public:
        SWalkTowardsPlayer() {
            queries[0] = Query(CVelocity::get_id(), CPosition::get_id(), CEnemy::get_id());
            queries[1] = Query(CPosition::get_id(), CPlayer::get_id());
            query_count = 2;
        }

        void update(ECS &ecs) {
            auto enemy_entities = get_query(0)->get_entities();
            auto player_entities = get_query(1)->get_entities();
            auto player = player_entities->first();
            auto player_pos = ecs.get_component<CPosition>(*player);
            Iterator it = { .next = 0 };
            Entity e;
            while (enemy_entities->next(it, e)) {
                CPosition& enemy_pos = ecs.get_component<CPosition>(e);
                CVelocity& enemy_vel = ecs.get_component<CVelocity>(e);
                CEnemy& enemy = ecs.get_component<CEnemy>(e);

                float dx = player_pos.x - enemy_pos.x;
                float dy = player_pos.y - enemy_pos.y;
                float dist = sqrt(dx * dx + dy * dy);
                if (dist > 0) {
                    enemy_vel.x = dx / dist;
                    enemy_vel.y = dy / dist;
                }
            }
        }
};

class SCollide : public System<SCollide> {
    public:
        SCollide() {
            queries[0] = Query(CPosition::get_id(), CSize::get_id());
            queries[1] = Query(CPosition::get_id(), CSize::get_id());
            query_count = 2;
        }

        void update(ECS &ecs) {
            auto entities_a = get_query(0)->get_entities();
            auto entities_b = get_query(1)->get_entities();
            Iterator it_a = { .next = 0 };
            Iterator it_b = { .next = 0 };
            Entity e_a, e_b;
            while (entities_a->next(it_a, e_a)) {
                CPosition& pos_a = ecs.get_component<CPosition>(e_a);
                CSize& size_a = ecs.get_component<CSize>(e_a);
                while (entities_b->next(it_b, e_b)) {
                    // Skip self-collision
                    if (e_a == e_b) continue;
                    CPosition& pos_b = ecs.get_component<CPosition>(e_b);
                    CSize& size_b = ecs.get_component<CSize>(e_b);

                    if (pos_a.x < pos_b.x + size_b.width &&
                        pos_a.x + size_a.width > pos_b.x &&
                        pos_a.y < pos_b.y + size_b.height &&
                        pos_a.y + size_a.height > pos_b.y) {
                        // Collision detected
                        printf("Collision detected between entity %d and entity %d\n", e_a, e_b);
                    }
                }
            }
        }
};

class SRender : public System<SRender> {
    public:
        SRender() {
            queries[0] = Query(CPosition::get_id(), CName::get_id());
            query_count = 1;
        }

        void update(ECS &ecs) {
            auto entities = get_query(0)->get_entities();
            Iterator it = { .next = 0 };
            Entity e;
            while (entities->next(it, e)) {
                CPosition& pos = ecs.get_component<CPosition>(e);
                CName& name = ecs.get_component<CName>(e);
                printf("Rendering entity %s at position (%f, %f)\n", name.name, pos.x, pos.y);
            }
        }
};

int main() {
    ECS ecs = ECS();
    ecs.register_component<CPlayer>();
    ecs.register_component<CPosition>();
    ecs.register_component<CVelocity>();
    ecs.register_component<CEnemy>();
    ecs.register_component<CSize>();
    ecs.register_component<CName>();
    auto move_system = ecs.register_system<SMove>();
    auto walk_system = ecs.register_system<SWalkTowardsPlayer>();
    auto collide_system = ecs.register_system<SCollide>();
    auto render_system = ecs.register_system<SRender>();

    Entity player = ecs.create_entity();
    ecs.add_component<CPlayer>(player, CPlayer());
    ecs.add_component<CPosition>(player, CPosition{.x = 1, .y = 0});
    ecs.add_component<CVelocity>(player, CVelocity{.x = 0.6, .y = 0.1});
    ecs.add_component<CSize>(player, CSize{.width = .2, .height = .2});
    ecs.add_component<CName>(player, CName{.name = "Player"});
    Entity enemy = ecs.create_entity();
    ecs.add_component<CEnemy>(enemy, CEnemy());
    ecs.add_component<CPosition>(enemy, CPosition{.x = 0, .y = 0});
    ecs.add_component<CVelocity>(enemy, CVelocity{.x = 0, .y = 0});
    ecs.add_component<CSize>(enemy, CSize{.width = .2, .height = .2});
    ecs.add_component<CName>(enemy, CName{.name = "Enemy"});

    for (int i = 0; i < 15; i++) {
        move_system->update(ecs);
        walk_system->update(ecs);
        collide_system->update(ecs);
        render_system->update(ecs);
    }
}


