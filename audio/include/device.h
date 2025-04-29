#pragma once

#include "effects/spatializer.h"
#include "source.h"
#include "sources.h"

#define SAMPLE_RATE 44100
#define CHANNELS 2

struct audio_device {
    struct device_sources sources;
    struct audio_listener listener;
    ma_device_info info;
    ma_device device;

    uint32_t active_source_index;
    char is_playing;
};

struct device_list {
    ma_device_info * devices;
    uint32_t device_count;
};

struct audio_device * init_device(ma_device_info device_info, struct source_list * sources, ma_context * context, ma_device_data_proc callback);
struct device_sources init_device_sources(struct source_list * source_list);
struct audio_listener init_device_listener();

void device_add_source(struct audio_device * device, struct sound_source source);
void device_toggle_play(struct audio_device * device);
void device_set_position(struct audio_device * device, Vec3 position);

struct device_list get_available_devices(ma_context * context);
ma_device_info get_default_device(struct device_list list);
