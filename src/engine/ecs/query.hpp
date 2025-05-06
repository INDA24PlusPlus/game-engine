#include "engine/ecs/component.hpp"
#include "engine/ecs/entityarray.hpp"
#include "engine/ecs/signature.hpp"
class Query {
    private:
        Signature signature;
    public:
        Query() {
            signature = Signature();
        }
        template <typename... ComponentIDs>
        Query(ComponentIDs... component_ids) {
            signature = Signature();
            int i = 0;
            for (auto component_id : {component_ids...}) {
                set_signature(signature, component_id);
            }
        }
        void add_entity(Entity entity) {
            entities.insert(entity);
        }

        void remove_entity(Entity entity) {
            entities.remove(entity);
        }

        EntityArray *get_entities() {
            return &entities;
        }

        Signature get_signature() {
            return signature;
        }

        bool is_in(Entity entity) {
            return entities.is_in(entity);
        }
        EntityArray entities;
};
