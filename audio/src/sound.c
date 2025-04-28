#include "sound.h"
#include "wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sound_data init_sound(FILE * fp, const struct wav_file wav) {
    const uint32_t buf_capacity = wav.data.data_size < MAX_SOUND_BUF_SIZE
        ? wav.data.data_size
        : MAX_SOUND_BUF_SIZE;

    struct sound_data audio = {
        .fp = fp,
        .buf_index = 0,
        .buf_end_index = 0,
        .buf_capacity = buf_capacity,
        .buf = malloc(sizeof(SOUND_BUF_TYPE) * buf_capacity)};

    sound_read(&audio, OVERWRITE);

    return audio;
}

void sound_read(struct sound_data * audio, enum sound_stream_read_type read_type) {
    if (read_type == APPEND && audio->buf_index < audio->buf_end_index) {
        // Move the elements left in the buffer to the front of the buffer
        memmove(audio->buf, &audio->buf[audio->buf_index], (audio->buf_end_index - audio->buf_index) * sizeof(SOUND_BUF_TYPE));
        audio->buf_end_index = (audio->buf_end_index - audio->buf_index);

        // Get the amount of free space after the buffer
        uint32_t free_space = audio->buf_capacity - audio->buf_end_index;

        // Fill the remainder of the buffer
        audio->buf_end_index += fread(&audio->buf[audio->buf_end_index], sizeof(SOUND_BUF_TYPE), free_space, audio->fp);
    } else if (read_type == OVERWRITE) {
        audio->buf_end_index = fread(audio->buf, sizeof(SOUND_BUF_TYPE), audio->buf_capacity, audio->fp);
    } else {
        puts("Invalid audio_read???");
        exit(1);
    }

    if (audio->buf_end_index < audio->buf_capacity && !feof(audio->fp)) {
        puts("An error occured while reading audio stream");
    }

    audio->buf_index = 0;
}
