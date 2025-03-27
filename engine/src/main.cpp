#include "types.h"
#include "utils.hpp"
#include <set>

// Info:
// Very basic implementation, currently does not make use of any data structures
// more complex than a stack. Need to implement more data structures to 
// efficiently both store data as well as iterate over it faster.

// Todo
// [ ] Make EntityManager, ComponentManager & SystemManager work together
// [ ] Separate the code into different files
// [ ] Packing data more efficiently
// [ ] Implement hash map
// [ ] Look into how to implement a better way to iterate over entities


using Entity = u32;
const u32 MAX_ENTITIES = 1000;

using ComponentID = u32;
const u32 MAX_COMPONENTS = 32;

using Signature = u64;

Signature set_signature(Signature signature, ComponentID component_id) {
    return signature | (1 << component_id);
}

Signature remove_signature(Signature signature, ComponentID component_id) {
    return signature & ~(1 << component_id);
}

class Component {
    protected:
        static u32 id_counter;
};

u32 Component::id_counter = 0;

template <typename T>
class ComponentArray : public Component {
public:
    ComponentArray() {
        for (Entity entity = 0; entity < MAX_ENTITIES; entity++) {
            data[entity] = T();
        }
    }

    T& get_component(Entity entity) {
        return data[entity];
    }

    void set_component(Entity entity, T component) {
        data[entity] = component;
    }

    static u32 get_id() {
        static u32 id = id_counter++;
        return id;
    }

    private:
        T data[MAX_ENTITIES];
        static bool registered;

};

class EntityManager {
public:
    EntityManager() {
        entity_count = 0;
        for (Entity entity = 0; entity < MAX_ENTITIES; entity++) {
            available_entities.push(entity);
        }
    }

    Entity create_entity() {
        Entity entity = available_entities.pop();
        entity_count++;
        return entity;
    }

    void destroy_entity(Entity entity) {
        available_entities.push(entity);
        entity_count--;
    }

    void set_signature(Entity entity, Signature signature) {
        signatures[entity] = signature;
    }

    Signature get_signature(Entity entity) {
        return signatures[entity];
    }
private:
    Stack<Entity> available_entities = Stack<Entity>(MAX_ENTITIES);
    Signature signatures[MAX_ENTITIES];
    u32 entity_count;
};

class ComponentManager {
public:
    template <typename T>
    void register_component() {
        const u32 component_id = T::get_id();
        component_arrays[component_id] = ComponentArray<T>();
        component_count++;
    }

    template <typename T>
    void add_component(Entity entity, T component) {
        const u32 component_id = T::get_id();
        component_arrays[component_id].set_component(entity, component);
    }

    template <typename T>
    T& get_component(Entity entity) {
        const u32 component_id = T::get_id();
        return component_arrays[component_id].get_component(entity);
    }

    template <typename T>
    void remove_component(Entity entity) {
        const u32 component_id = T::get_id();
        component_arrays[component_id].set_component(entity, T());
    }

    template <typename T>
    ComponentID get_component_id() {
        return T::get_id();
    }

    void destroy_entity(Entity entity) {
        for (u32 i = 0; i < component_count; i++) {
            component_arrays[i].set_component(entity, Component());
        }
    }
private:
    ComponentArray<Component> component_arrays[MAX_COMPONENTS];
    u32 component_count;
};

class SystemBase {
    protected:
        static u32 id_counter;
};

u32 SystemBase::id_counter = 0;

const u32 MAX_SYSTEMS = 128;

class System : public SystemBase {
    public:
        // Temporary, I think this should be possible without but currently no
        // time to implement
        std::set<Entity> entities;
        u32 get_id() {
            static u32 id = id_counter++;
            return id;
        }
};

class SystemManager {
    public:
        template <typename T>
        void register_system() {
            const u32 system_id = T::get_id();
            assert(system_id < MAX_SYSTEMS, "System ID is too large");
            systems[system_id] = T();
            system_count++;
        }

        template <typename T>
        T& get_system() {
            const u32 system_id = T::get_id();
            return systems[system_id];
        }

        void destroy_entity(Entity entity) {
            for (u32 i = 0; i < system_count; i++) {
                System system = systems[i];
                if (system.entities.find(entity) != system.entities.end()) {
                    system.entities.erase(entity);
                }
            }
        }

    private:
        System systems[MAX_COMPONENTS];
        u32 system_count;
        Signature signatures[MAX_ENTITIES];
};

class ECS {
    public:
        ECS() {
            entity_manager = EntityManager();
            component_manager = ComponentManager();
            system_manager = SystemManager();
        }

        Entity create_entity() {
            return entity_manager.create_entity();
        }

        void destroy_entity(Entity entity) {
            entity_manager.destroy_entity(entity);
            component_manager.destroy_entity(entity);
            system_manager.destroy_entity(entity);
        }
    private:
        EntityManager entity_manager;
        ComponentManager component_manager;
        SystemManager system_manager;
};
