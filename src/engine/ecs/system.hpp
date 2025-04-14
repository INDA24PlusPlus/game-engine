#pragma once

#include "entity.hpp"
#include "types.h"
#include <set>

const u32 MAX_SYSTEMS = 128;

class SystemBase {
    protected:
        static u32 id_counter;
    public:
        std::set<Entity> entities;
};

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
        SystemManager();

        ~SystemManager();

        template <typename T>
            T* register_system() {
                const u32 system_id = T::get_id();
                assert(system_id < MAX_SYSTEMS, "System ID is too large");
                systems[system_count] = new T();
                signatures[system_count] = 0;
                system_count++;
                return (T*)systems[system_count - 1];
            }

        void destroy_entity(Entity entity);

        void update_components(Entity entity, Signature signature);

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
