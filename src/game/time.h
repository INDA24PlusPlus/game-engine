#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"

class RDeltaTime : public Resource<RDeltaTime> {
public:
  f32 delta_time;
  f32 prev_time;
};

class SDeltaTime : public System<SDeltaTime> {
public:
  SDeltaTime() {}
  void update(ECS &ecs);
};
