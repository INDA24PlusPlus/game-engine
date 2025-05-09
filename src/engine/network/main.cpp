#include "../ecs/ecs.hpp"
#include "../ecs/resource.hpp"
#include "GLFW/glfw3.h"
#include "engine/ecs/entity.hpp"
#include "engine/ecs/system.hpp"
#include "engine/utils/logging.h"
#include "game/components.h"
#include "game/enemy.h"
#include "game/player.h"
#include "game/state.h"
#include "game/time.h"
#include "game/util.h"
#include "glm/fwd.hpp"
#include "server.hpp"
#include <cmath>
#include <cstdio>

class SEnemyGhost : public System<SEnemyGhost> {
public:
  SEnemyGhost() {
    queries[0] = Query(CTranslation::get_id(), CVelocity::get_id(),
                       CEnemyGhost::get_id(), CSpeed::get_id());
    queries[1] = Query(CClient::get_id());
    query_count = 2;
  }

  void update(ECS &ecs) {
    auto time = ecs.get_resource<RDeltaTime>();

    auto entities = get_query(0)->get_entities();
    Iterator it = {.next = 0};
    Entity e;
    while (entities->next(it, e)) {
      CTranslation &translation = ecs.get_component<CTranslation>(e);
      CVelocity &vel = ecs.get_component<CVelocity>(e);
      CEnemyGhost &ghost = ecs.get_component<CEnemyGhost>(e);
      float &speed = ecs.get_component<CSpeed>(e).speed;
      // float& health = ecs.get_component<CHealth>(e).health;

      // TODO: Get nearest player instead of only player in single player
      Entity nearest_player;
      float old_dist = MAXFLOAT;
      auto players = get_query(1)->get_entities();
      Iterator player_it = {.next = 0};
      Entity player;
      while (players->next(player_it, player)) {
        auto player_pos = ecs.get_component<CClient>(player);
        auto dist_x = abs(player_pos.x - translation.pos.x);
        auto dist_z = abs(player_pos.z - translation.pos.z);
        auto dist = dist_x + dist_z;
        if (dist < old_dist) {
          nearest_player = player;
          old_dist = dist;
        }
      }

      if (old_dist == MAXFLOAT) {
        return;
      }

      auto nearest_player_client = ecs.get_component<CClient>(nearest_player);
      glm::vec3 nearest_player_translation =
          glm::vec3(nearest_player_client.x, 0, nearest_player_client.z);
      // CHealth &nearest_player_health =
      //     ecs.get_component<CHealth>(nearest_player);

      // update enemy
      if (ghost.cooldown > 0) {
        ghost.cooldown -= time->delta_time;
        // INFO("debug {}", ghost.cooldown);
        continue;
      }

      glm::vec3 enemy_target_look =
          nearest_player_translation - translation.pos;
      enemy_target_look.y = 0;

      float player_distance = glm::length(enemy_target_look);

      if (player_distance > 2.0f) {
        // Rotate
        enemy_target_look = glm::normalize(enemy_target_look);
        glm::quat target_rotation =
            glm::quatLookAt(enemy_target_look, glm::vec3(0, 1, 0));
        translation.rot = glm::slerp(translation.rot, target_rotation,
                                     time->delta_time * 2.0f);

        // Move in facing direction
        glm::vec3 forward = translation.rot * glm::vec3(0, 0, -1);
        vel.vel = forward * speed * time->delta_time;

        // hover effect sine function
        translation.pos.y = glm::sin(time->prev_time * speed) * 0.3;
        // printf("%f\n", time->delta_time);
        // printf("%f\n", translation.pos.y);
      } else {
        // Damage player
        // nearest_player_health.take_damage(20);
        ghost.cooldown = 1.0;
      }
    }
  }
};

int main(int argc, char *argv[]) {
  glfwInit();
  ECS ecs = ECS();

  INFO("Register components");
  ecs.register_component<CClient>();
  ecs.register_component<CTranslation>();
  ecs.register_component<CEnemyGhost>();
  ecs.register_component<CVelocity>();
  ecs.register_component<CSpeed>();

  INFO("Register systems");
  auto accept_client_system = ecs.register_system<SAcceptClients>();
  auto player_send_system = ecs.register_system<SSendPositions>();
  auto get_system = ecs.register_system<SGetPositions>();
  auto enemy_ghost_system = ecs.register_system<SEnemyGhost>();
  auto move_system = ecs.register_system<SMove>();
  auto send_enemy_system = ecs.register_system<SSendEnemyPositions>();
  auto delta_time_system = ecs.register_system<SDeltaTime>();

  INFO("Register resources");
  ecs.register_resource(new RServer(argv[1]));
  ecs.register_resource(new RDeltaTime(glfwGetTime()));

  for (int i = 0; i < 2; i++) {
    INFO("Create ghost");
    Entity ghost = ecs.create_entity();
    ecs.add_component(ghost, CEnemyGhost{.id = i, .cooldown = 0.0f});
    ecs.add_component(ghost, CSpeed{.speed = (float)(3 + i)});
    ecs.add_component(ghost,
                      CTranslation{.pos = glm::vec3(10.0f, 0.0f, i * 10.0f),
                                   .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                   .scale = glm::vec3(1.0f)});
    ecs.add_component(ghost, CVelocity());
  }

  while (true) {
    delta_time_system->update(ecs);
    accept_client_system->update(ecs);
    enemy_ghost_system->update(ecs);
    move_system->update(ecs);
    get_system->update(ecs);
    player_send_system->update(ecs);
    send_enemy_system->update(ecs);
  }
}
