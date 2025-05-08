#pragma once
#include <filesystem>

#include "engine/audio/source/reverb.h"
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
        CSoundData data;
        ReverbEffect reverb = ReverbEffect(1.0, 0.8, 0.8, 0.3);
        enum SoundState state;
        u8 volume;
    public:
        CAudioSource() {};
        CAudioSource(const std::filesystem::path path) 
            : state(SoundState::Playing), volume(100)
        {
            auto wav = WavFile(path);
            data = CSoundData(wav.fp, wav.data.file_offset, wav.data.size);
        }

        void play(SOUND_BUF_TYPE * output, u32 frame_count, float scale) {
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
                    SOUND_BUF_TYPE sample = reverb.process_sample_with_reverb(data.at_offset(i * CHANNELS + c));

#ifdef PCMS16LE
                    i32 mixed = (int32_t)output[i * CHANNELS + c] + (int32_t)sample * scale /* * spatializer.pan[c] */;
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
            data.set_playback(0);
        }

        inline bool is_playing() {
            return state == SoundState::Playing;
        }
};
