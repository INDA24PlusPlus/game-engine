#include "components.h"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/system.hpp"
SMove::SMove() {
  queries[0] = Query(CTranslation::get_id(), CVelocity::get_id());
  query_count = 1;
}

void SMove::update(ECS &ecs) {
  auto entities = get_query(0)->get_entities();
  Iterator it = {.next = 0};
  Entity e;
  while (entities->next(it, e)) {
    CTranslation &translation = ecs.get_component<CTranslation>(e);
    CVelocity &vel = ecs.get_component<CVelocity>(e);
    translation.pos += vel.vel;
    vel.vel = glm::vec3(0);
  }
}
