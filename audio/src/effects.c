#include "effects.h"
#include "sound.h"
#include <stdlib.h>

struct sound_effects init_sound_effects() {
    return (struct sound_effects) {.effects = NULL, .effects_capacity = 0};
}

void sound_effects_append(struct sound_effects * effects, struct sound_effect effect) {
    if (effects == NULL) {
        puts("Null sound_effects in append");
        exit(1);
    }

    if (effects->effects_count == effects->effects_capacity) {
        if (effects->effects_capacity == 0) {
            effects->effects_capacity = 1;
            effects->effects = malloc(sizeof(struct sound_effect) * effects->effects_capacity);
        } else {
            effects->effects_capacity *= 2;
            effects->effects = realloc(effects->effects, sizeof(struct sound_effect) * effects->effects_capacity);
        }
    }

    effects->effects[effects->effects_count++] = effect;
}

#define INIT_SOUND_EFFECT_CASE(TYPE, ENUM_NAME) case ENUM_NAME: return (struct sound_effect) {.effect = (union sound_effect_union) {.comb_filter = * (struct TYPE *)data, }, .type = ENUM_NAME};
struct sound_effect init_sound_effect(void * data, enum sound_effect_type type) {
    switch (type) {
        EFFECTS_LIST(INIT_SOUND_EFFECT_CASE)
    }
}

#define UPDATE_SOUND_EFFECT_CASE(TYPE, ENUM_NAME) case ENUM_NAME: return update_##TYPE(&effect->effect.TYPE, value);
inline SOUND_BUF_TYPE update_sound_effect(struct sound_effect * effect, SOUND_BUF_TYPE value) {
    switch (effect->type) {
        EFFECTS_LIST(UPDATE_SOUND_EFFECT_CASE)
    }
}
