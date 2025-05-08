#pragma once

#include "engine/ecs/component.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct Player {
  glm::vec3 position;
  glm::quat rotation;
  glm::vec3 scale;
  float health;
  float speed;

  Player();

  void take_damage(float damage);
  bool is_alive() const;
};

class CPlayer : public Component<CPlayer> {
public:
};

class CLocal : public Component<CLocal> {
public:
};
