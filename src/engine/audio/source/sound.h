#pragma once
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <miniaudio.h>
#include <filesystem>
#include <utility>

#include "engine/audio/source/wav.h"
#include "engine/core.h"
#include "engine/ecs/component.hpp"
#include "engine/audio/constants.h"
#include "engine/utils/logging.h"

enum SoundStreamReadType { Append, Overwrite };

class CSoundData : public Component<CSoundData> {
    private:
        std::shared_ptr<std::ifstream> stream = nullptr;
        u32 file_data_offset;
        u32 size;
    public:
        CSoundData() {};
        CSoundData(const std::filesystem::path path) {
            stream = std::make_shared<std::ifstream>(path, std::ios_base::binary);
            
            if (!stream->is_open() || !(*stream)) {
                FATAL("Unable to open file: {}", path.string());
            }

            auto wav = WavFile(stream);
            this->file_data_offset = wav.data.file_offset;
            this->size = wav.data.size;
        }

        CSoundData(const CSoundData&) = default;
        CSoundData& operator=(CSoundData&&) = default;
        CSoundData& operator=(const CSoundData&) = default;

        inline bool read(SOUND_BUF_TYPE * buf, size_t samples_to_read) {
            if (stream->eof()) {
                FATAL("Read EOF");
            }

            stream->read((char *)buf, samples_to_read * sizeof(SOUND_BUF_TYPE));
            return (u32)stream->gcount() < samples_to_read * sizeof(SOUND_BUF_TYPE);
        }

        void set_playback(u32 offset) {
            stream->seekg(file_data_offset + offset * CHANNELS, std::ios::beg);
        }
};
