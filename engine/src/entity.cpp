#include "entity.hpp"

EntityManager::EntityManager() : available_entities(MAX_ENTITIES) {
    entity_count = 0;
    for (Entity entity = 0; entity < MAX_ENTITIES; entity++) {
        available_entities.push(entity);
    }
}

EntityManager::~EntityManager() {
}

Entity EntityManager::create_entity() {
    Entity entity = available_entities.pop();
    entity_count++;
    return entity;
}

void EntityManager::destroy_entity(Entity entity) {
    available_entities.push(entity);
    entity_count--;
}

void EntityManager::set_signature(Entity entity, Signature signature) {
    signatures[entity] = signature;
}

Signature EntityManager::get_signature(Entity entity) {
    return signatures[entity];
}

FIFO<Entity> available_entities = FIFO<Entity>(MAX_ENTITIES);
Signature signatures[MAX_ENTITIES];
u32 entity_count;
