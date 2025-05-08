#pragma once
#include <math.h>
#include <cstdlib>

#include "engine/audio/constants.h"
#include "engine/ecs/component.hpp"
#include "engine/utils/logging.h"

#define DELAY_LINES 4

static const i8 HADAMARD_MATRIX[DELAY_LINES][DELAY_LINES] = {
    {1,  1,  1,  1},
    {1, -1,  1, -1},
    {1,  1, -1, -1},
    {1, -1, -1,  1},
};

class ReverbEffect {
    private:
        SOUND_BUF_TYPE * delay_lines[DELAY_LINES];
        u32 delay_lengths[DELAY_LINES];
        u32 delay_indices[DELAY_LINES] = {0};
        SOUND_BUF_TYPE filtered_samples[DELAY_LINES] = {0};

        float room_size;
        float decay;
        float damping;
        float wet_k;

        bool enabled = true;
    public:
        ReverbEffect(float room_size, float decay, float damping, float wet_k)
            : room_size(room_size), decay(decay), damping(damping), wet_k(wet_k)
        {
            static_assert(DELAY_LINES <= 4, "Update base delays array after having changed updated DELAY_LINES");
            u32 base_delays[DELAY_LINES] = {29, 37, 47, 59};
            float size_mult = 0.5f + room_size * 4.5f;

            for (size_t i = 0; i < DELAY_LINES; ++i) {
                delay_lengths[i] = (SAMPLE_RATE * base_delays[i] * size_mult) / 1000;

                if (SAMPLE_RATE < delay_lengths[i]) {
                    FATAL("Delay length has overstrided the max delay of 1 second");
                }

                delay_lines[i] = (SOUND_BUF_TYPE *) calloc(delay_lengths[i], sizeof(SOUND_BUF_TYPE));
            }
        }

        SOUND_BUF_TYPE process_sample_with_reverb(SOUND_BUF_TYPE sample) {
            if (!enabled) {
                return sample;
            }

#ifdef PCMS16LE
            i32 wet_samples[DELAY_LINES];
            for (size_t i = 0; i < DELAY_LINES; ++i) {
                wet_samples[i] = delay_lines[i][delay_indices[i]];
            }

            // Apply damping to each sample
            for (size_t i = 0; i < DELAY_LINES; ++i) {
                filtered_samples[i] = (1.0f - damping) * wet_samples[i] + damping * filtered_samples[i];
            }

            // Mix samples through Hadamard matrix for FDN
            i32 mixed_samples[DELAY_LINES] = {0};
            for (size_t i = 0; i < DELAY_LINES; ++i) {
                for (size_t j = 0; j < DELAY_LINES; ++j) {
                    mixed_samples[i] += HADAMARD_MATRIX[i][j] * filtered_samples[j];
                }
                // Normalize for energy preservation
                mixed_samples[i] = mixed_samples[i] / sqrtf(DELAY_LINES);
            }


            // Update state
            const float feedback_scale = fminf(room_size, 1.0f) * decay;
            for (size_t i = 0; i < DELAY_LINES; ++i) {
                // Apply feedback gain
                i32 feedback = (i32)(mixed_samples[i] * feedback_scale) + (i32)sample;
                CLAMP_VALUE(feedback);

                delay_lines[i][delay_indices[i]] = feedback;
                delay_indices[i] = (delay_indices[i] + 1) % delay_lengths[i];
            }

            i32 wet_sample = 0;
            for (size_t i = 0; i < DELAY_LINES; ++i) {
                wet_sample += filtered_samples[i];
            }

            wet_sample /= DELAY_LINES;
            CLAMP_VALUE(wet_sample);
#else
            static_assert(0, "process_reverb does not support encoding");
#endif

            return (1.0f - wet_k) * sample + wet_sample * wet_k;
        }
};
