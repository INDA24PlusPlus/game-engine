#pragma once

#include "engine/ecs/component.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
class CEnemyGhost : public Component<CEnemyGhost> {
public:
  int id;
  float cooldown = 0.0f;
};
