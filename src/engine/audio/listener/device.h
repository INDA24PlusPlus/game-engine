#pragma once
#include <miniaudio.h>
#include "engine/audio/audio.h"
#include "engine/audio/source/sound.h"
#include "engine/audio/source/source.h"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/utils/logging.h"

#define SAMPLE_RATE 44100
#define CHANNELS 2

class CMADevice : public Component<CMADevice> {
    public:
        ma_device device;
};

class CMADeviceInfo : public Component<CMADeviceInfo> {
    public:
        ma_device_info info;
    public:
        CMADeviceInfo() {};
        CMADeviceInfo(ma_device_info info) : info(info) {};
};

class CAudioDevice : public Component<CAudioDevice> {
    private:
        Entity m_sources;
        Entity m_listener;
        Entity m_info;
        Entity m_device;

        bool is_playing;
    public:
        CAudioDevice() {};
        CAudioDevice(ECS &ecs, ma_context * context, ma_device_info info) {
            this->is_playing = true;

            ecs.register_component<CAudioSource>();
            ecs.register_component<CMADeviceInfo>();
            ecs.register_component<CMADevice>();

            ecs.add_component(m_info, CMADeviceInfo(info));
            initialize_ma_device(ecs, context);
        }

        void deinit(ECS &ecs) {
            auto device = ecs.get_component<CMADevice>(m_device);
            ma_device_stop(&device.device);
            ma_device_uninit(&device.device);
        }

        void toggle_play(ECS &ecs) {
            is_playing ^= true;

            auto device = ecs.get_component<CMADevice>(m_device);
            if (is_playing) {
                ma_device_start(&device.device);
            } else {
                ma_device_stop(&device.device);
            }
        }
    private:
        void initialize_ma_device(ECS &ecs, ma_context * context) {
            auto info = ecs.get_component<CMADeviceInfo>(m_info);

            ma_device_config config = ma_device_config_init(ma_device_type_playback);
            config.playback.pDeviceID = &info.info.id;
            config.playback.format = ma_format_s16;
            config.playback.channels = CHANNELS;
            config.sampleRate = SAMPLE_RATE;
            config.dataCallback = &data_callback;
            config.pUserData = &ecs;
            
            ecs.add_component(m_device, CMADevice());
            auto device = ecs.get_component<CMADevice>(m_device);

            if (ma_device_init(context, &config, &device.device) != MA_SUCCESS) {
                ma_device_uninit(&device.device);
                FATAL("An error occured while initializing device \"{}\"", info.info.name);
            }

            if (is_playing) {
                ma_device_start(&device.device);
            }
        }
};

struct device_list {
    ma_device_info * devices;
    uint32_t device_count;
};

static struct device_list get_available_devices(ma_context * context) {
    struct device_list list;

    if (ma_context_get_devices(context, &list.devices, &list.device_count, NULL, NULL) != MA_SUCCESS) {
        puts("An error occured trying to retrieve devices");
    }

    return list;
}

static ma_device_info get_default_device(struct device_list list) {
    const uint32_t list_length = list.device_count;
    for (uint32_t i = 0; i < list_length; ++i) {
        if (list.devices[i].isDefault) {
            return list.devices[i];
        }
    }

    puts("There was no default audio device?!!?");
    exit(1);
}
