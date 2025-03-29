#include "types.h"
#include "a.h"
#include "utils.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <thread>
#include <typeinfo>

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
const u32 MAX_ENTITIES = 100;

using ComponentID = u32;
const u32 MAX_COMPONENTS = 32;

using Signature = u64;

Signature set_signature(Signature signature, ComponentID component_id) {
    return signature | (1 << component_id);
}

Signature remove_signature(Signature signature, ComponentID component_id) {
    return signature & ~(1 << component_id);
}

class ComponentBase {
    protected:
        static u32 id_counter;
    public:
        static u32 get_id() {
            return 0;
        }
};

u32 ComponentBase::id_counter = 0;

template class FIFO<Entity>;

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

class EntityManager {
    public:
        EntityManager() {
            entity_count = 0;
            for (Entity entity = 0; entity < MAX_ENTITIES; entity++) {
                available_entities.push(entity);
            }
        }

        ~EntityManager() {
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
        FIFO<Entity> available_entities = FIFO<Entity>(MAX_ENTITIES);
        Signature signatures[MAX_ENTITIES];
        u32 entity_count;
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

class SystemBase {
    protected:
        static u32 id_counter;
    public:
        // Temporary, I think this should be possible without but currently no
        // time to implement
        std::set<Entity> entities;
};

u32 SystemBase::id_counter = 0;

const u32 MAX_SYSTEMS = 128;

template <typename T>
class System : public SystemBase {
    public:
        static u32 get_id() {
            static u32 id = id_counter++;
            return id;
        }
};

class SystemManager {
    public:
        SystemManager() {
            system_count = 0;
        }

        ~SystemManager() {
            for (u32 i = 0; i < system_count; i++) {
                delete systems[i];
            }
        }
        template <typename T>
            T* register_system() {
                const u32 system_id = T::get_id();
                assert(system_id < MAX_SYSTEMS, "System ID is too large");
                systems[system_count] = new T();
                signatures[system_count] = 0;
                system_count++;
                return (T*)systems[system_count - 1];
            }

        void destroy_entity(Entity entity) {
            for (u32 i = 0; i < system_count; i++) {
                SystemBase *system = systems[i];
                if (system->entities.find(entity) != system->entities.end()) {
                    system->entities.erase(entity);
                }
            }
        }

        void update_components(Entity entity, Signature signature) {
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

        template <typename T>
            void set_signature(Signature signature) {
                const u32 system_id = T::get_id();
                signatures[system_id] = signature;
            }

    private:
        SystemBase *systems[MAX_SYSTEMS];
        Signature signatures[MAX_SYSTEMS];
        u32 system_count;

        template <typename T>
            System<T> *get_system(u32 system_id) {
                return (System<T>*)systems[system_id];
            }
};

class ECS {
    public:
        ECS() {
            entity_manager = new EntityManager();
            component_manager = new ComponentManager();
            system_manager = new SystemManager();
        }

        Entity create_entity() {
            return entity_manager->create_entity();
        }

        void destroy_entity(Entity entity) {
            entity_manager->destroy_entity(entity);
            component_manager->destroy_entity(entity);
            system_manager->destroy_entity(entity);
        }

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

class Position : public Component<Position> {
    public:
        f32 x;
        f32 y;
};

class Velocity : public Component<Velocity> {
    public:
        f32 x;
        f32 y;
};

class Iteration : public Component<Iteration> {
    public:
        f32 value;
};

class MovementSystem : public System<MovementSystem> {
    public:
        void update(ECS& ecs) {
            for (Entity entity : entities) {
                Position& position = ecs.get_component<Position>(entity);
                Velocity& velocity = ecs.get_component<Velocity>(entity);
                position.x += velocity.x;
                position.y += velocity.y;
            }
        }
};

class VaryVelocitySystem : public System<VaryVelocitySystem> {
    public:
        void update(ECS& ecs) {
            for (Entity entity : entities) {
                Velocity& velocity = ecs.get_component<Velocity>(entity);
                Iteration& iteration = ecs.get_component<Iteration>(entity);
                velocity.x = cos(iteration.value);
                velocity.y = sin(iteration.value);
                iteration.value += 0.1f;
            }
        }
};

class A : public System<A> {
    public:
        void print() {
        }
};

class B : public System<B> {
    public:
        void print() {
        }
};

void run() {
    ECS ecs = ECS();
    ecs.register_component<Position>();
    ecs.register_component<Velocity>();
    ecs.register_component<Iteration>();

    MovementSystem *movement_system = ecs.register_system<MovementSystem>();
    VaryVelocitySystem *vary_velocity_system = ecs.register_system<VaryVelocitySystem>();

    Signature movement_signature = 0;
    movement_signature = set_signature(movement_signature, Position::get_id());
    movement_signature = set_signature(movement_signature, Velocity::get_id());
    ecs.set_system_signature<MovementSystem>(movement_signature);

    Signature vary_velocity_signature = 0;
    vary_velocity_signature = set_signature(vary_velocity_signature, Velocity::get_id());
    vary_velocity_signature = set_signature(vary_velocity_signature, Iteration::get_id());
    ecs.set_system_signature<VaryVelocitySystem>(vary_velocity_signature);

    Entity entity = ecs.create_entity();

    Position position = Position();
    position.x = 1.0f;
    position.y = 2.0f;

    Velocity velocity = Velocity();
    velocity.x = 1.0f;
    velocity.y = 1.0f;

    Iteration iteration = Iteration();
    iteration.value = 0.0f;

    ecs.add_component(entity, position);
    ecs.add_component(entity, velocity);
    ecs.add_component(entity, iteration);

    while (true) {
        movement_system->update(ecs);
        vary_velocity_system->update(ecs);
        Position& new_position = ecs.get_component<Position>(entity);
        printf("Position: %f %f\n", new_position.x, new_position.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int main() {
    test_a();
    run();
    return 0;
}
