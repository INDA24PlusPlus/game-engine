#pragma once
#include <filesystem>

#include "engine/audio/source/sound.h"
#include "engine/audio/source/wav.h"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/system.hpp"
#include "engine/core.h"
#include "engine/audio/constants.h"

enum SoundState { Paused, Playing, Finished };

class CAudioSource : public Component<CAudioSource> {
    private:
        Entity m_data;
        enum SoundState state;
        u8 volume;
    public:
        CAudioSource() {};
        CAudioSource(const std::filesystem::path path) 
            : state(SoundState::Playing), volume(100)
        {
            ECS::register_component<CSoundData>();
            m_data = ECS::create_entity();

            auto wav = WavFile(path);
            ECS::add_component(m_data, CSoundData(wav.fp, wav.data.file_offset, wav.data.size));
        }

        void play(SOUND_BUF_TYPE * output, u32 frame_count, float scale) {
            CSoundData &data = ECS::get_component<CSoundData>(m_data);

            if (data.get_remaining_frames() < frame_count * CHANNELS) {
                data.read(SoundStreamReadType::Append);

                // check that there were enough frames left
                if (data.get_remaining_frames() < frame_count * CHANNELS) {
                    frame_count = data.get_remaining_frames() / CHANNELS;
                    state = SoundState::Finished;
                }
            }

            static_assert(CHANNELS == 2, "We only support stereo sound due to spatializer pan handling");
            for (size_t i = 0; i < frame_count; ++i) {
                for (size_t c = 0; c < CHANNELS; ++c) {
                    SOUND_BUF_TYPE sample = data.at_offset(i * CHANNELS + c);

#ifdef PCMS16LE
                    i32 mixed = (int32_t)output[i * CHANNELS + c] + (int32_t)sample * scale/* * spatializer.pan[c] */;
#else
                    static_assert(0, "Encoding is not supported");
#endif
                    CLAMP_VALUE(mixed);
                    output[i * CHANNELS + c] += mixed;
                }
            }

            data.update_index(frame_count * CHANNELS);
        }

        void restart() {
            CSoundData &data = ECS::get_component<CSoundData>(m_data);
            data.set_playback(0);
        }

        inline bool is_playing() {
            return state == SoundState::Playing;
        }
};
