#pragma once

#include "engine/core.h"
#include "engine/ecs/component.hpp"
#include "engine/utils/logging.h"
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <cstdio>

#define WAV_ENFORCE_VALID_CHUNK_ID(UNKNOWN_ID, ID) if (strncmp(UNKNOWN_ID, ID, 4)) {FATAL("Invalid chunk ID \"{:4}\", should be \"{:4}\"\n", UNKNOWN_ID, ID); }

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
    u32 file_offset;
    u32 size;
};

class WavFile {
    public:
        FILE * fp;
        struct wav_file_header header;
        struct wav_file_data data;
    public:
        WavFile(const std::filesystem::path path)
            : fp(fopen(path.c_str(), "rb"))
        {
            if (fp == NULL) {
                FATAL("Unable to open file \"{}\"", path.root_path().c_str());
            }

            struct wav_file_chunk riff_chunk = get_chunk();
            WAV_ENFORCE_VALID_CHUNK_ID(riff_chunk.chunk_id, "RIFF");

            char format[4];
            fread(&format, 1, sizeof(format), fp);
            WAV_ENFORCE_VALID_CHUNK_ID(format, "WAVE");

            struct wav_file_chunk fmt_chunk = get_chunk();
            WAV_ENFORCE_VALID_CHUNK_ID(fmt_chunk.chunk_id, "fmt ");

            fread(&header, 1, sizeof(header), fp);
            // check that header is correctly formatted

            if (!(header.block_align == (header.num_channels * header.bits_per_sample / 8) &&
                    header.byte_rate == (header.sample_rate * header.block_align))) {
                FATAL("¿Corrupt? .wav format section");
            }

            if (header.audio_format != 1 && header.bits_per_sample != 16) {
                FATAL("We only support pcm_s16le wav files. Fix: ffmpeg -i [file] -c:a pcm_s16le [output]");
            }

            if (fmt_chunk.chunk_size == 18 || fmt_chunk.chunk_size == 40) {
                uint16_t extension_size;
                fread(&extension_size, 1, sizeof(extension_size), fp);
                fseek(fp, extension_size, SEEK_CUR);
            } else if (fmt_chunk.chunk_size != 16) {
                FATAL("Invalid .wav file");
            }

            struct wav_file_chunk data_chunk = get_chunk();
            WAV_ENFORCE_VALID_CHUNK_ID(data_chunk.chunk_id, "data");

            data.size = data_chunk.chunk_size;
            data.file_offset = ftell(fp);
        };
    private:
        struct wav_file_chunk get_chunk() {
            struct wav_file_chunk chunk;
            fread(&chunk, 1, sizeof(chunk), fp);
            return chunk;
        }
};
