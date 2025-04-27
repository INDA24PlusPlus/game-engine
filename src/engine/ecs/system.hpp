#pragma once

#include "engine/utils/logging.h"
#include "entity.hpp"
#include "types.h"
#include "entityarray.hpp"
#include "query.hpp"

const u32 MAX_SYSTEMS = 128;

class SystemBase {
    protected:
        static u32 id_counter;
    public:
        static u32 get_id();
        Query queries[4];
        u32 query_count;

        SystemBase() {
        }
};

class ECS;

template <typename T>
class System: public SystemBase {
    private:

    public:
        static u32 get_id() {
            static u32 id = id_counter++;
            if (id >= MAX_SYSTEMS) {
                ERROR("System ID exceeds maximum systems");
            }
            return id;
        }

        bool update(ECS &ecs);

        Query *get_query(u32 index) {
            return &queries[index];
        }
};
