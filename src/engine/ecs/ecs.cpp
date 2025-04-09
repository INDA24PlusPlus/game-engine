#include "ecs.hpp"

ECS::ECS() {
    entity_manager = new EntityManager();
    component_manager = new ComponentManager();
    system_manager = new SystemManager();
}

Entity ECS::create_entity() {
    return entity_manager->create_entity();
}

void ECS::destroy_entity(Entity entity) {
    entity_manager->destroy_entity(entity);
    component_manager->destroy_entity(entity);
    system_manager->destroy_entity(entity);
}
