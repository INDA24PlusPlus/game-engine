#pragma once

#include "engine/core.h"
#include "engine/ecs/component.hpp"
#include "engine/utils/logging.h"
#include <cstdio>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <ios>
#include <memory>

#define WAV_ENFORCE_VALID_CHUNK_ID(UNKNOWN_ID, ID) if (strncmp(UNKNOWN_ID, ID, 4)) {FATAL("Invalid chunk ID \"{:4}\", should be \"{:4}\"\n", UNKNOWN_ID, ID); }
#define READ_FROM_STREAM(stream, dest) (stream)->read((char *) (&dest), sizeof(dest))

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
        struct wav_file_header header;
        struct wav_file_data data;
    public:
        WavFile(std::shared_ptr<std::ifstream> &stream) {
            struct wav_file_chunk riff_chunk = get_chunk(stream);
            WAV_ENFORCE_VALID_CHUNK_ID(riff_chunk.chunk_id, "RIFF");

            char format[4];
            READ_FROM_STREAM(stream, format);
            WAV_ENFORCE_VALID_CHUNK_ID(format, "WAVE");

            struct wav_file_chunk fmt_chunk = get_chunk(stream);
            WAV_ENFORCE_VALID_CHUNK_ID(fmt_chunk.chunk_id, "fmt ");

            READ_FROM_STREAM(stream, header);

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
                READ_FROM_STREAM(stream, extension_size);
                stream->seekg(extension_size, std::ios::cur);
            } else if (fmt_chunk.chunk_size != 16) {
                FATAL("Invalid .wav file");
            }

            struct wav_file_chunk data_chunk = get_chunk(stream);
            WAV_ENFORCE_VALID_CHUNK_ID(data_chunk.chunk_id, "data");

            data.size = data_chunk.chunk_size;
            data.file_offset = static_cast<u32>(stream->tellg());
        };
    private:
        struct wav_file_chunk get_chunk(std::shared_ptr<std::ifstream> &stream) {
            struct wav_file_chunk chunk;
            stream->read((char *)&chunk, sizeof(chunk));
            return chunk;
        }
};
