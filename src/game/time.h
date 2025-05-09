#include "GLFW/glfw3.h"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"

class RDeltaTime : public Resource<RDeltaTime> {
public:
  f32 delta_time;
  f32 prev_time;
  RDeltaTime(f32 current_time) : delta_time(0), prev_time(current_time) {}
};

class SDeltaTime : public System<SDeltaTime> {
public:
  SDeltaTime();
  void update(ECS &ecs);
};
