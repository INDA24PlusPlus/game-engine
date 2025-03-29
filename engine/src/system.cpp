#include "system.hpp"

u32 SystemBase::id_counter = 0;

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
        if (system->entities.find(entity) != system->entities.end()) {
            system->entities.erase(entity);
        }
    }
}

void SystemManager::update_components(Entity entity, Signature signature) {
    for (u32 i = 0; i < system_count; i++) {
        SystemBase *system = systems[i];
        Signature system_signature = signatures[i];
        if ((signature & system_signature) == system_signature) {
            system->entities.insert(entity);
        } else {
            system->entities.erase(entity);
        }
    }
}
