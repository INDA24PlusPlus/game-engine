#pragma once
#include "device.h"
#include "source.h"
#include "sources.h"
#include <miniaudio.h>

struct audio_context {
    struct audio_device * device;
    struct source_list sources;
    ma_device_data_proc callback;
    ma_context context;
};

struct audio_context * init_audio();

void audio_set_device(struct audio_context * context, ma_device_info device);

void audio_add_sound(struct audio_context * context, struct sound_source source);
