#pragma once
#include <filesystem>
#include <fstream>

#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/system.hpp"
#include "engine/core.h"

#define MAX_SOUND_BUF_SIZE 50 * 8 * 1024 // 50KB
#define SOUND_BUF_TYPE int16_t
#define PCMS16LE // remove this if SOUND_BUF_TYPE is no longer int16_t

enum SoundState { Paused, Playing, Finished };

class CAudioSource : public Component<CAudioSource> {
    private:
        std::ifstream stream;
        enum SoundState state;
        u8 volume;
    public:
        CAudioSource() {};
        CAudioSource(const std::filesystem::path path) {}

        // Refresh audio data buffer
        void update(ECS &ecs) {

        }
};
