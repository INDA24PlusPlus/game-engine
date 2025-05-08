#include "systemmanager.hpp"

SystemManager::SystemManager() {
    system_count = 0;
}

SystemManager::~SystemManager() {
    for (u32 i = 0; i < system_count; i++) {
        delete systems[i];
    }
}

void SystemManager::destroy_entity(Entity entity) {
    for (u32 i = 0; i < system_count; i++) {
        SystemBase *system = systems[i];
        // Iterate through every query in system
        for (u32 j = 0; j < system->query_count; j++) {
            Query *query = &system->queries[j];
            // Check if entity is in query
            if (query->entities.is_in(entity)) {
                // Remove entity from query
                query->entities.remove(entity);
            }
        }
    }
}

void SystemManager::update_components(Entity entity, Signature signature) {
    for (u32 i = 0; i < system_count; i++) {
        SystemBase *system = systems[i];
        for (u32 j = 0; j < system->query_count; j++) {
            Query *query = &system->queries[j];
            Signature query_signature = query->get_signature();
            if ((signature & query_signature) == query_signature) {
#ifndef ECS_DEBUGGING
                INFO("Added entity {} to query, {}", entity, j);
#endif
                // Entity matches query
                query->entities.insert(entity);
            } else {
#ifndef ECS_DEBUGGING
                INFO("Removed entity {} from query ", entity);
#endif
                // Entity does not match query
                query->entities.remove(entity);
            }
        }
    }
}

