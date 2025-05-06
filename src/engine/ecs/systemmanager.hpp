#include "engine/ecs/entity.hpp"
#include "engine/ecs/system.hpp"
class SystemManager {
    public:
        SystemManager();

        ~SystemManager();

        template <typename T>
            T* register_system() {
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

        void update();
    private:
        SystemBase *systems[MAX_SYSTEMS];
        Signature signatures[MAX_SYSTEMS];
        u32 system_count;

        template <typename T>
            System<T> *get_system(u32 system_id) {
                return (System<T>*)systems[system_id];
            }
};
