#include "entityarray.hpp"
#include "engine/ecs/entity.hpp"
#include "engine/utils/logging.h"

// Packed array of entities
EntityArray::EntityArray() {
    for (u32 i = 0; i < MAX_ENTITIES; i++) {
        idxs[i] = -1;
        data[i] = -1;
    }
    head = 0;
    size = 0;
}

EntityArray::~EntityArray() {
}

void EntityArray::insert(Entity e) {
    if (idxs[e] != -1) return;
    data[head] = e;
    idxs[e] = head;
    head++;
}

void EntityArray::remove(Entity e) {
    if (idxs[e] == -1) return;
    // IDX av entity
    // Ta bort entity
    // Ta bort IDX
    // Om IDX != head - 1 && head - 1 != 0
    //   E2 = data[head - 1]
    //   data[idx] = E2
    //   data[head - 1] = 0
    //   idxs[E2] = idx

    u32 idx = idxs[e];
    idxs[e] = -1;
    if (idx != head - 1 && head - 1 != 0) {
        Entity e2 = data[head - 1];
        data[idx] = e2;
        data[head - 1] = 0;
        idxs[e2] = idx;
    }

    if (!(head - 1 < 0)) {
        head--;
    }
}

bool EntityArray::is_in(Entity e) {
    return idxs[e] != -1;
}

bool EntityArray::next(Iterator &it, Entity &e) {
    if (it.next == head) return false;
    e = data[it.next];
    it.next++;
    return true;
}

Entity *EntityArray::first() {
    if (head == 0) return nullptr;
    return &data[0];
}
