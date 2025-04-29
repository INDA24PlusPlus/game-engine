#pragma once
#include <stdint.h>

#include "filters/comb.h"

#define EFFECTS_LIST(f) \
    f(comb_filter, COMB_FILTER) \
    // f(reverb, REVERB)

#define GET_ENUM_NAME_LIST(_, enum_name) enum_name,
#define GET_TYPE_UNION_DEF(type, _) struct type type;

struct sound_effect {
    union sound_effect_union {
        EFFECTS_LIST(GET_TYPE_UNION_DEF)
    } effect;

    enum sound_effect_type {
        EFFECTS_LIST(GET_ENUM_NAME_LIST)
    } type;
};

struct sound_effects {
    struct sound_effect * effects;
    uint32_t effects_count;
    uint32_t effects_capacity;
};

struct sound_effects init_sound_effects();
void sound_effects_append(struct sound_effects * effects, struct sound_effect effect);

struct sound_effect init_sound_effect(void * data, enum sound_effect_type type);
SOUND_BUF_TYPE update_sound_effect(struct sound_effect * effect, SOUND_BUF_TYPE value);
