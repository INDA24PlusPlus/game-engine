#pragma once
#include <miniaudio.h>
#include <stdint.h>

#include "sound.h"

enum sound_state {
    PAUSED,
    PLAYING,
    FINISHED
};

struct sound_source {
    struct sound_data audio;
    enum sound_state state;
    uint8_t volume;
};

struct sound_source init_source(const char * file_path);
void source_play_sound(struct sound_source * sound, SOUND_BUF_TYPE * output, uint32_t frames, float scale);

void source_set_volume(struct sound_source * source, uint8_t volume);
