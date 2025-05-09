#include "player.h"
#include "time.h"
#include "components.h"
#include "util.h"
#include "state.h"
#include "engine/ecs/ecs.hpp"

SPlayerController::SPlayerController() {
  queries[0] = Query(CTranslation::get_id(), CVelocity::get_id(),
                     CLocal::get_id(), CPlayer::get_id());
  query_count = 1;
}

void SPlayerController::update(ECS &ecs) {
  // INFO("debug");
  float delta_time = ecs.get_resource<RDeltaTime>()->delta_time;
  auto scene = ecs.get_resource<RScene>();
  engine::Camera &camera = scene->m_camera;

  RInput &input = *ecs.get_resource<RInput>();
  // Settings settings = ecs.get_resource<RInput>()->settings;

  // Get local player from query
  Entity player;
  {
    auto player_ptr = get_query(0)->get_entities()->first();
    if (player_ptr == nullptr) {
      ERROR("NO PLAYER FOUND!!");
      return;
    }
    player = *player_ptr;
  }

  CVelocity &velocity = ecs.get_component<CVelocity>(player);
  float &player_speed = ecs.get_component<CSpeed>(player).speed;
  CTranslation &translation = ecs.get_component<CTranslation>(player);

  // movement input
  glm::vec3 direction = input.get_direction();

  // player movement
  if (!input.input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
    auto forward = camera.m_orientation * glm::vec3(0, 0, -1);
    auto right = camera.m_orientation * glm::vec3(1, 0, 0);
    forward.y = 0;
    right.y = 0;

    forward = glm::normalize(forward);
    right = glm::normalize(right);
    velocity.vel = (right * direction.x + forward * direction.z) *
                   player_speed * delta_time;
    ;

    if (glm::length(direction) > 0) {
      glm::quat target_rotation =
          glm::quatLookAt(glm::normalize(velocity.vel), glm::vec3(0, 1, 0));

      translation.rot =
          glm::slerp(translation.rot, target_rotation, delta_time * 8.0f);
    }
  }
}
