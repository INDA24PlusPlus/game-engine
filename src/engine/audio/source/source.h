#pragma once
#include <cmath>
#include <cstring>
#include <filesystem>

#include "engine/audio/listener/listener.h"
#include "engine/audio/source/reverb.h"
#include "engine/audio/source/sound.h"
#include "engine/audio/source/wav.h"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/system.hpp"
#include "engine/core.h"
#include "engine/audio/constants.h"
#include "engine/utils/logging.h"

enum SoundState { Paused, Playing, Restart, Finished };

class CAudioSource : public Component<CAudioSource> {
    private:
        CSoundData data;
        ReverbEffect reverb = ReverbEffect(1.0, 0.8, 0.8, 0.3);
        glm::vec3 * position;

        enum SoundState state;
        u8 volume;
    public:
        CAudioSource() : position(nullptr) {};
        CAudioSource(const std::filesystem::path path, glm::vec3 * position = nullptr) 
            : position(position), state(SoundState::Playing), volume(100)
        {
            this->data = CSoundData(path);
        }

        void play(AudioListener &listener, SOUND_BUF_TYPE * output, u32 frame_count, float scale) {
            switch (this->state) {
                case SoundState::Restart:
                    this->restart();
                    // This is meant to fall through
                case SoundState::Playing: break;
                default: return;
            }

            SOUND_BUF_TYPE buf[frame_count * CHANNELS];
            if (this->data.read(buf, frame_count * CHANNELS)) {
                this->state = SoundState::Finished;
            }

            scale *= std::powf(volume / 100.0f, 2.0f);

            if (position != nullptr) {
                listener.update(*this->position, this->volume);
                DEBUG("pan: [{}, {}]", listener.pans[0], listener.pans[1]);
                scale *= listener.volume / 100.0f;
            }

            static_assert(CHANNELS == 2, "We only support stereo sound due to spatializer pan handling");
            for (size_t i = 0; i < frame_count; ++i) {
                for (size_t c = 0; c < CHANNELS; ++c) {
                    SOUND_BUF_TYPE sample = this->reverb.process_sample_with_reverb(buf[i * CHANNELS + c]);

#ifdef PCMS16LE
                    i32 mixed = (int32_t)output[i * CHANNELS + c] + (int32_t)sample * scale;

                    if (this->position != nullptr) {
                        mixed *= listener.pans[c];
                    }
#else
                    static_assert(0, "Encoding is not supported");
#endif
                    CLAMP_VALUE(mixed);
                    output[i * CHANNELS + c] += mixed;
                }
            }
        }

        inline SoundState get_state() {
            return this->state;
        }

    private:
        void restart() {
            DEBUG("Restarting playback");
            data.set_playback(0);
            this->state = SoundState::Playing;
        }
};
