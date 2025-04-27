#include "ecs.hpp"
#include "engine/ecs/resource.hpp"

ECS::ECS() {
    entity_manager = new EntityManager();
    component_manager = new ComponentManager();
    system_manager = new SystemManager();
    resource_manager = new ResourceManager();
}

Entity ECS::create_entity() {
    Entity e = entity_manager->create_entity();
    Signature signature = entity_manager->get_signature(e);
    system_manager->update_components(e, signature);
    return e;
}

void ECS::destroy_entity(Entity entity) {
    entity_manager->destroy_entity(entity);
    component_manager->destroy_entity(entity);
    system_manager->destroy_entity(entity);
}
