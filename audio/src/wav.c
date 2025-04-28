#include "wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENFORCE_VALID_ID(UNKNOWN_ID, ID) if (strncmp(UNKNOWN_ID, ID, 4)) {printf("Invalid chunk ID \"%.4s\", should be \"%.4s\"\n", UNKNOWN_ID, ID); return INVALID_WAVE_FILE; }

struct wav_file_chunk get_chunk(FILE * fp) {
    struct wav_file_chunk chunk;
    fread(&chunk, 1, sizeof(chunk), fp);
    return chunk;
}

struct wav_file parse_wav_file(FILE * fp) {
    struct wav_file_chunk riff_chunk = get_chunk(fp);
    ENFORCE_VALID_ID(riff_chunk.chunk_id, "RIFF");

    char format[4];
    fread(&format, 1, sizeof(format), fp);
    ENFORCE_VALID_ID(format, "WAVE");

    struct wav_file_chunk fmt_chunk = get_chunk(fp);
    ENFORCE_VALID_ID(fmt_chunk.chunk_id, "fmt ");

    struct wav_file_header header;
    fread(&header, 1, sizeof(header), fp);

    // check that header is in correct
    if (!(header.block_align == (header.num_channels * header.bits_per_sample / 8) &&
            header.byte_rate == (header.sample_rate * header.block_align))) {
        puts("¿Corrupt? .wav format section");
        return INVALID_WAVE_FILE;
    }

    if (header.audio_format != 1 && header.bits_per_sample != 16) {
        puts("We only support pcm_s16le wav files. Fix: ffmpeg -i [file] -c:a pcm_s16le [output]");
        exit(1);
    }

    if (fmt_chunk.chunk_size == 18 || fmt_chunk.chunk_size == 40) {
        uint16_t extension_size;
        fread(&extension_size, 1, sizeof(extension_size), fp);
        fseek(fp, extension_size, SEEK_CUR);
    } else if (fmt_chunk.chunk_size != 16) {
        puts("Invalid .wav file");
        return INVALID_WAVE_FILE;
    }

    struct wav_file_chunk data_chunk = get_chunk(fp);
    printf("chunk_size = %d\n", data_chunk.chunk_size);
    ENFORCE_VALID_ID(data_chunk.chunk_id, "data");

    struct wav_file_data data = {
        .data_size = data_chunk.chunk_size,
        .data_start_offset = ftell(fp)
    };

    return (struct wav_file) {header, data};
}
