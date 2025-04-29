#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "sound.h"
#include "source.h"
#include "sources.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    struct audio_device * device = pDevice->pUserData;
    memset(pOutput, 0, frameCount * CHANNELS * sizeof(SOUND_BUF_TYPE));

    if (!device->is_playing) {
        return;
    }

    const struct source_list * sources = device->sources.list;
    const uint32_t sources_count = sources->source_count;

    uint32_t active_sources = 0;
    for (uint32_t i = 0; i < sources_count; ++i) {
        active_sources += source_list_at(sources, i)->state == PLAYING;
    }

    const float scale = active_sources == 0
        ? 1.0f
        : 1.0f / active_sources;
 
    for (uint32_t i = 0; i < sources_count; ++i) {
        struct sound_source * source = source_list_at(sources, i);
        if (source->state == PLAYING) {
            source_play_sound(source, pOutput, &device->listener, frameCount, scale);
        }
    }    
}

struct audio_context * init_audio() {
    // appears that miniaudio requires the device to be stored at the same address
    // as it is called with when initializing so I am just heap allocating this?
    struct audio_context * context = malloc(sizeof(*context));
    context->sources = init_source_list();
    context->callback = data_callback;
    context->device = NULL;

    if (ma_context_init(NULL, 0, NULL, &context->context) != MA_SUCCESS) {
        puts("Unable to initialize miniaudio context");
        exit(1);
    }

    ma_device_info default_device = get_default_device(get_available_devices(&context->context));
    audio_set_device(context, default_device);

    return context;
}

void audio_set_device(struct audio_context * context, ma_device_info device) {
    if (context->device != NULL) {
        ma_device_stop(&context->device->device);
        ma_device_uninit(&context->device->device);
    }

    context->device = init_device(device, &context->sources, &context->context, context->callback);
    if (ma_device_start(&context->device->device) != MA_SUCCESS) {
        puts("Failed to start playback device");
        ma_device_uninit(&context->device->device);
        exit(1);
    }
}

void audio_add_sound(struct audio_context * context, struct sound_source source) {
    source_list_append(&context->sources, source);
}
