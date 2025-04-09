#pragma once

#include "types.h"
#include "signature.hpp"
#include "utils.hpp"

using Entity = u32;

const u32 MAX_ENTITIES = 1000;

class EntityManager {
    public:
        EntityManager();

        ~EntityManager();

        Entity create_entity();

        void destroy_entity(Entity entity);

        void set_signature(Entity entity, Signature signature);

        Signature get_signature(Entity entity);
    private:
        FIFO<Entity> available_entities;
        Signature signatures[MAX_ENTITIES];
        u32 entity_count;
};
