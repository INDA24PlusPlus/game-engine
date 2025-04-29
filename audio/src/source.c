#include "source.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "sound.h"
#include "wav.h"

#define TO_STR(x) #x
#define TO_STR_VALUE(x) TO_STR(x)

struct sound_source init_source(const char * file_path) {
    FILE * fp = fopen(file_path, "rb");

    if (fp == NULL) {
        printf("Unable to open file \"%s\"\n", file_path);
        exit(1);
    }

    struct wav_file wav = parse_wav_file(fp);
    const struct wav_file invalid = INVALID_WAVE_FILE;

    if (memcmp(&invalid, &wav, sizeof(invalid)) == 0) {
        puts("Invalid wav file source. Try: ffmpeg -i [file] -bitexact [new_file_name]");
        exit(1);
    }

    if (wav.header.sample_rate != SAMPLE_RATE) {
        puts("Incorrect sample rate. The rate should be " TO_STR_VALUE(SAMPLE_RATE));
        exit(1);
    }

    return (struct sound_source) {.audio = init_sound(fp, wav), .volume = 100, .state = PLAYING};
}

void source_play_sound(struct sound_source * sound, SOUND_BUF_TYPE * output, uint32_t frames, float scale) {
    // make sure that we have enough frames to play
    uint32_t frames_left = sound->audio.buf_end_index - sound->audio.buf_index;
    if (frames_left < frames * CHANNELS) {
        sound_read(&sound->audio, APPEND);

        // check that there were enough frames left
        frames_left = sound->audio.buf_end_index - sound->audio.buf_index;
        if (frames_left < frames * CHANNELS) {
            frames = frames_left / CHANNELS;
            sound->state = FINISHED;
        }
    }

    scale *= powf(sound->volume / 100.0f, 2.0f);

    for (uint32_t i = 0; i < frames; ++i) {
        for (uint32_t c = 0; c < CHANNELS; ++c) {
            SOUND_BUF_TYPE sample = sound->audio.buf[sound->audio.buf_index + i * CHANNELS + c];

#ifdef _pcms16le
            int32_t mixed = (int32_t)output[i * CHANNELS + c] + (int32_t)sample * scale;

            if (INT16_MAX < mixed) {
                mixed = INT16_MAX;
            } else if (mixed < INT16_MIN) {
                mixed = INT16_MIN;
            }
#else
            assert(0);
#endif

            output[i * CHANNELS + c] += mixed;
        }
    }

    sound->audio.buf_index += frames * CHANNELS;
}

void source_set_volume(struct sound_source * source, uint8_t volume) {
    source->volume = volume;
}
