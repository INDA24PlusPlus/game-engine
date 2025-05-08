#pragma once
#include <cstdio>
#include <cstring>
#include <miniaudio.h>

#include "engine/core.h"
#include "engine/ecs/component.hpp"
#include "engine/utils/logging.h"
#include "engine/audio/constants.h"

enum SoundStreamReadType { Append, Overwrite };

class CSoundData : public Component<CSoundData> {
    private:
        FILE * fp;
        u32 index = 0;
        u32 read_end_index = 0;
        u32 file_data_offset;
        u32 size;
        SOUND_BUF_TYPE buf[MAX_SOUND_BUF_SIZE] = {0};
    public:
        CSoundData() {};
        CSoundData(FILE * fp, u32 file_data_offset, u32 size) : fp(fp), file_data_offset(file_data_offset), size(size) {
            read(SoundStreamReadType::Overwrite);
        }

        void read(enum SoundStreamReadType readtype) {
            if (readtype == SoundStreamReadType::Append && index < read_end_index) {
                // Move the elements left in the buffer to the front of the buffer
                memmove(buf, &buf[index], (read_end_index - index) * sizeof(SOUND_BUF_TYPE));
                read_end_index -= index;

                // Get the amount of free space after the non-read buffered data
                size_t free_space = size - read_end_index;

                // Fill the remainder of the buffer
                read_end_index += fread(&buf[read_end_index], sizeof(SOUND_BUF_TYPE), free_space, fp);
            } else if (readtype == SoundStreamReadType::Overwrite) {
                read_end_index = fread(buf, sizeof(SOUND_BUF_TYPE), MAX_SOUND_BUF_SIZE, fp);
            } else {
                FATAL("Invalid CSoundData::read!");
            }

            // Check 
            if (read_end_index < size && feof(fp) == EOF) {
                ERROR("An error occured while reading sound data");
            } 

            index = 0;
        }

        void update_index(u32 index) {
            this->index += index;
        }

        void set_playback(u32 offset) {
            index = 0;
            read_end_index = 0;
            fseek(fp, file_data_offset + offset * CHANNELS, SEEK_SET);
            read(SoundStreamReadType::Overwrite);
        }

        inline SOUND_BUF_TYPE at_offset(size_t offset) {
            return buf[index + offset];
        }

        inline u32 get_free_space() {
            return size - read_end_index;
        }

        inline u32 get_remaining_frames() {
            return read_end_index - index;
        }
};
