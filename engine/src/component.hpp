#pragma once

#include "types.h"
#include "entity.hpp"

const u32 MAX_COMPONENTS = 32;

using ComponentID = u32;

class ComponentBase {
    protected:
        static u32 id_counter;
    public:
        static u32 get_id();
};

template <typename T>
class Component : public ComponentBase {
    public:
        static u32 get_id() {
            static u32 id = id_counter++;
            return id;
        }

        void print() {
            T *t = (T*)this;
            t->print();
        }
};

class IComponentArray {
    public:
        virtual ~IComponentArray() = default;
        virtual void destroy_entity(Entity entity) = 0;
};

template <typename T>
class ComponentArray {
    public:
        ComponentArray() {
            for (Entity entity = 0; entity < MAX_ENTITIES; entity++) {
                data[entity] = T();
            }
        }

        T& get_component(Entity entity) {
            T& c = (T&)data[entity];
            return c;
        }

        void set_component(Entity entity, T component) {
            data[entity] = component;
        }

        void destroy_entity(Entity entity) {
            data[entity] = T();
        }

    private:
        T data[MAX_ENTITIES];
};

class ComponentManager {
    public:
        ComponentManager() {
            component_count = 0;
            for (u32 i = 0; i < MAX_COMPONENTS; i++) {
                component_arrays[i] = nullptr;
            }
        }

        ~ComponentManager() {
            for (u32 i = 0; i < component_count; i++) {
                delete component_arrays[i];
            }
        }

        template <typename T>
            void register_component() {
                const u32 component_id = T::get_id();
                ComponentArray<ComponentBase> *component_array = (ComponentArray<ComponentBase>*) new ComponentArray<T>();
                component_arrays[component_id] = component_array;
                component_count++;
            }

        template <typename T>
            void add_component(Entity entity, T component) {
                const u32 component_id = T::get_id();
                get_component_array<T>()->set_component(entity, component);
            }

        template <typename T>
            T& get_component(Entity entity) {
                ComponentArray<T> *component_array = get_component_array<T>();
                T& component = component_array->get_component(entity);
                return component;
            }

        template <typename T>
            void remove_component(Entity entity) {
                get_component_array<T>()->set_component(entity, T());
            }

        template <typename T>
            ComponentID get_component_id() {
                return T::get_id();
            }

        void destroy_entity(Entity entity) {
            for (u32 i = 0; i < component_count; i++) {
                component_arrays[i]->destroy_entity(entity);
            }
        }
    private:
        ComponentArray<ComponentBase> *component_arrays[MAX_COMPONENTS];
        u32 component_count;

        template <typename T>
            ComponentArray<T> *get_component_array() {
                const u32 component_id = T::get_id();
                return (ComponentArray<T>*)component_arrays[component_id];
            }
};
