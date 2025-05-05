#pragma once

#include "entity.hpp"

struct Iterator {
    u32 next;
};

class EntityArray {
    private:
        Entity data[MAX_ENTITIES];
        u32 idxs[MAX_ENTITIES];
        u32 head;
        u32 size;
    public:
        EntityArray();

        ~EntityArray();

        void insert(Entity e);

        void remove(Entity e);

        bool is_in(Entity e);

        bool next(Iterator &it, Entity &e);

        Entity *first();
};
