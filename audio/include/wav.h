#pragma once

#include <stdint.h>
#include <stdio.h>


struct wav_file_chunk {
    char chunk_id[4];
    uint32_t chunk_size;
};

struct wav_file_header {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

struct wav_file_data {
    uint32_t data_start_offset;
    uint32_t data_size;
    void * buf;
};

struct wav_file {
    struct wav_file_header header;
    struct wav_file_data data;
};

#define INVALID_WAVE_FILE ((struct wav_file) {0});

struct wav_file parse_wav_file(FILE * fp);
