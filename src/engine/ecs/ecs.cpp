#include "ecs.hpp"
#include "engine/ecs/entity.hpp"
#include "engine/ecs/resource.hpp"

EntityManager ECS::entity_manager = EntityManager();
ComponentManager ECS::component_manager = ComponentManager();
SystemManager ECS::system_manager = SystemManager();
ResourceManager ECS::resource_manager = ResourceManager();
