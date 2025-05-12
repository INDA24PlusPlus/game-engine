#pragma once

#include "engine/Camera.h"
#include "engine/audio/constants.h"
#include "glm/geometric.hpp"
#include <glm/vec3.hpp>

class AudioListener {
    private:
        glm::vec3 position;
        glm::vec3 forward;
        glm::vec3 up;
    public:
        float pans[CHANNELS]; // [0]: Left, [1]: Right
        float volume;
    public:
        AudioListener() {}

        void update(glm::vec3 source_position, float volume) {
            glm::vec3 direction = source_position - position;
            float distance = direction.length();
            glm::vec3 direction_norm = glm::normalize(direction);

            this->volume = volume / distance;
            if (this->volume < AUDIO_VOLUME_CUTOFF) {
                this->volume = 0.0f;
            }

            glm::vec3 listener_right = glm::normalize(glm::cross(forward, up));
            float pan = glm::dot(direction_norm, listener_right);

            pans[0] = 0.5f - pan * 0.5f;
            pans[1] = 0.5f + pan * 0.5f;
        }

        void set_position(glm::vec3 position) {
            this->position = position;
        }

        void set_forward(glm::vec3 forward) {
            this->forward = forward;
        }

        void set_up(glm::vec3 up) {
            this->up = up;
        }
};
