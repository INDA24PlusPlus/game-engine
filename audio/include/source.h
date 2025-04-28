#pragma once
#include <miniaudio.h>
#include <stdint.h>

#include "sound.h"

struct sound_source {
    struct sound_data audio;
    char play;
    ma_vec3f world_position;
};

struct sound_source init_source(const char * file_path);
void source_play_sound(struct sound_source * sound, SOUND_BUF_TYPE * output, uint32_t frames, float scale);
