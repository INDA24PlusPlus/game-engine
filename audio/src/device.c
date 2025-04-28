#include "device.h"

#include "source.h"
#include "sources.h"
#include <stdlib.h>

// =======================
// |    Initialization   |
// =======================
struct audio_device * init_device(ma_device_info device_info, struct source_list * sources, ma_context * context, ma_device_data_proc callback) {
    struct audio_device * device = calloc(1, sizeof(struct audio_device));
    device->sources = init_device_sources(sources);
    device->info = device_info;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = &device->info.id;
    config.playback.format = ma_format_s16;
    config.playback.channels = CHANNELS;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = callback;
    config.pUserData = device;

    if (ma_device_init(context, &config, &device->device) != MA_SUCCESS) {
        printf("An error occured while initializing device \"%s\"\n", device->info.name);
        exit(1);
    }

    return device;
}

struct device_sources init_device_sources(struct source_list * source_list) {
    return (struct device_sources) {.list = source_list};
}

// =================
// |    Methods    |
// =================
void device_add_source(struct audio_device * device, struct sound_source source) {
    source_list_append(device->sources.list, source);
}

void device_toggle_play(struct audio_device * device) {
    device->is_playing ^= 1;
}

// =================
// |    General    |
// =================
struct device_list get_available_devices(ma_context * context) {
    struct device_list list;

    if (ma_context_get_devices(context, &list.devices, &list.device_count, NULL, NULL) != MA_SUCCESS) {
        puts("An error occured trying to retrieve devices");
    }

    return list;
}

ma_device_info get_default_device(struct device_list list) {
    const uint32_t list_length = list.device_count;
    for (uint32_t i = 0; i < list_length; ++i) {
        if (list.devices[i].isDefault) {
            return list.devices[i];
        }
    }

    puts("There was no default audio device?!!?");
    exit(1);
}
