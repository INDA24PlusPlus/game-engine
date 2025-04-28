#pragma once

#include "source.h"

struct source_list {
    struct sound_source * sources;
    uint16_t source_count;
    uint16_t source_capacity;
};

struct device_sources {
    struct source_list * list;
};

#define SOURCE_INIT_CAPACITY 2

struct source_list init_source_list();

void source_list_append(struct source_list * list, struct sound_source source);
struct sound_source * source_list_at(const struct source_list * list, uint32_t index);
