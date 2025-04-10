#pragma once

#include "types.h"
#include "entity.hpp"
#include "component.hpp"
#include "system.hpp"
#include <cstdio>

class ECS {
    public:
        ECS();

        Entity create_entity();

        void destroy_entity(Entity entity);

        template <typename T>
        void register_component() {
            component_manager->register_component<T>();
        }

        template <typename T>
        void add_component(Entity entity, T component) {
            component_manager->add_component(entity, component);
            Signature signature = entity_manager->get_signature(entity);
            Signature new_signature = set_signature(signature, component_manager->get_component_id<T>());
            entity_manager->set_signature(entity, new_signature);
            system_manager->update_components(entity, new_signature);
        }

        template <typename T>
        void remove_component(Entity entity) {
            component_manager->remove_component<T>(entity);
            Signature signature = entity_manager->get_signature(entity);
            Signature new_signature = remove_signature(signature, component_manager->get_component_id<T>());
            entity_manager->set_signature(entity, new_signature);
            system_manager->update_components(entity, new_signature);
        }

        template <typename T>
        T& get_component(Entity entity) {
            return component_manager->get_component<T>(entity);
        }

        template <typename T>
        T *register_system() {
            return system_manager->register_system<T>();
        }

        template <typename T>
        void set_system_signature(Signature signature) {
            system_manager->set_signature<T>(signature);
        }
    private:
        EntityManager *entity_manager;
        ComponentManager *component_manager;
        SystemManager *system_manager;
};
