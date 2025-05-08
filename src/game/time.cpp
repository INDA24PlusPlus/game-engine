#include "time.h"
#include "GLFW/glfw3.h"
#include "engine/ecs/ecs.hpp"
void SDeltaTime::update(ECS &ecs) {
  auto time = ecs.get_resource<RDeltaTime>();
  time->delta_time = glfwGetTime() - time->prev_time;
  time->prev_time = glfwGetTime();
}
