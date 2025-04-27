#pragma once

#include "entity.hpp"

struct Iterator {
    u32 next;
};

class EntityArray {
    private:
        Entity* data;
        u32* idxs;
        u32 head;
        u32 size;
    public:
        EntityArray(u32 s);

        ~EntityArray();

        void insert(Entity e);

        void remove(Entity e);

        bool is_in(Entity e);

        bool next(Iterator &it, Entity &e);
};
