#pragma once
#include <stdint.h>
#include <stdio.h>

#include "wav.h"

#define MAX_SOUND_BUF_SIZE 32 * 1024 * 8 // 32KB
#define SOUND_BUF_TYPE int16_t
#define _pcms16le // remove this if SOUND_BUF_TYPE is no longer int16_t

enum sound_stream_read_type {
    APPEND,
    OVERWRITE
};

struct sound_data {
    FILE * fp;
    SOUND_BUF_TYPE * buf;
    uint32_t buf_index;
    uint32_t buf_end_index;
    uint32_t buf_capacity;
};

struct sound_data init_sound(FILE * fp, const struct wav_file wav);

void sound_refill(struct sound_data * audio);
void sound_read(struct sound_data * audio, enum sound_stream_read_type read_type);
