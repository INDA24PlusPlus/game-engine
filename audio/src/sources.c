#include "sources.h"
#include "source.h"
#include <stdlib.h>

struct source_list init_source_list() {
    return (struct source_list) {
        .source_count = 0,
        .source_capacity = SOURCE_INIT_CAPACITY,
        .sources = malloc(sizeof(struct sound_source) * SOURCE_INIT_CAPACITY)
    };
}

void source_list_append(struct source_list * list, struct sound_source source) {
    if (list == NULL) {
        puts("Invalid NULL list argument for call to source_list_append");
        exit(1);
    }

    if (list->source_capacity <= list->source_count) {
        list->source_capacity *= 2;
        list->sources = realloc(list->sources, sizeof(struct sound_source) * list->source_capacity);
    }

    list->sources[list->source_count++] = source;
}

struct sound_source * source_list_at(const struct source_list * list, uint32_t index) {
    if (list->source_count <= index) {
        printf("Index out of bounds error. Index %u in list of size %u\n", index, list->source_count);
        exit(1);
    }

    return &list->sources[index];
}
