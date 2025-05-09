#pragma once

#include "engine/ecs/component.hpp"
#include "engine/ecs/system.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

class CPlayer : public Component<CPlayer> {
public:
};

class CLocal : public Component<CLocal> {
public:
};

class CSpeed : public Component<CSpeed> {
public:
  float speed = 10;
};

class CHealth : public Component<CHealth> {
public:
  float health = 100;

  void take_damage(float damage) {
    health -= damage;
    if (health < 0.0f) {
      health = 0.0f;
    }
  }

  bool is_alive() { return health > 0.0f; }
};

class SPlayerController : public System<SPlayerController> {
public:
  SPlayerController();
  void update(ECS &ecs);
};
