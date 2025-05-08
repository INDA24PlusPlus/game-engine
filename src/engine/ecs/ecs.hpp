#pragma once

#include "engine/ecs/resource.hpp"
#include "engine/utils/logging.h"
#include "types.h"
#include "entity.hpp"
#include "component.hpp"
#include "systemmanager.hpp"

#define ECS_DEBUGGING

class ECS {
    private:
        static EntityManager entity_manager;
        static ComponentManager component_manager;
        static SystemManager system_manager;
        static ResourceManager resource_manager;
    public:
        ECS();

        static Entity create_entity() {
            Entity e = entity_manager.create_entity();
            Signature signature = entity_manager.get_signature(e);
            system_manager.update_components(e, signature);
            return e;
        }

        static void destroy_entity(Entity entity) {
            entity_manager.destroy_entity(entity);
            component_manager.destroy_entity(entity);
            system_manager.destroy_entity(entity);
        }

        template <typename T>
        static void register_component() {
            component_manager.register_component<T>();
        }

        template <typename T>
        static void add_component(Entity entity, T component) {
            component_manager.add_component(entity, component);
            Signature signature = entity_manager.get_signature(entity);
            Signature new_signature = set_signature(signature, component_manager.get_component_id<T>());
            entity_manager.set_signature(entity, new_signature);
            system_manager.update_components(entity, new_signature);
        }

        template <typename T>
        static void remove_component(Entity entity) {
            component_manager.remove_component<T>(entity);
            Signature signature = entity_manager.get_signature(entity);
            Signature new_signature = remove_signature(signature, component_manager.get_component_id<T>());
            entity_manager.set_signature(entity, new_signature);
            system_manager.update_components(entity, new_signature);
        }

        template <typename T>
        static T& get_component(Entity entity) {
            return component_manager.get_component<T>(entity);
        }

        template <typename T>
        static T *register_system() {
            return system_manager.register_system<T>();
        }

        template <typename T>
        static void set_system_signature(Signature signature) {
            system_manager.set_signature<T>(signature);
        }

        template <typename T>
        static T* register_resource(T* resource) {
            return resource_manager.register_resource(resource);
        }

        template <typename T>
        static T* get_resource() {
            return resource_manager.get_resource<T>();
        }
};
