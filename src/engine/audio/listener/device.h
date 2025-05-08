#pragma once
#include <cassert>
#include <miniaudio.h>
#include "engine/audio/source/source.h"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/utils/logging.h"

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
        Entity m_info = ECS::create_entity();
        Entity m_device = ECS::create_entity();

    public:
        bool is_playing;
    public:
        CAudioDevice() {};
        CAudioDevice(ma_context * context, ma_device_info info, ma_device_data_proc callback) {
            is_playing = true;

            ECS::register_component<CMADeviceInfo>();
            ECS::register_component<CMADevice>();

            ECS::add_component(m_info, CMADeviceInfo(info));
            initialize_ma_device(context, callback);
        }

        void deinit() {
            auto &device = ECS::get_component<CMADevice>(m_device);
            ma_device_stop(&device.device);
            ma_device_uninit(&device.device);
        }

        void toggle_play() {
            is_playing ^= true;

            auto &device = ECS::get_component<CMADevice>(m_device);
            if (is_playing) {
                assert(ma_device_start(&device.device) == MA_SUCCESS);
            } else {
                assert(ma_device_stop(&device.device) == MA_SUCCESS);
            }
        }
    private:
        void initialize_ma_device(ma_context * context, ma_device_data_proc callback) {
            auto info = ECS::get_component<CMADeviceInfo>(m_info);

            ma_device_config config = ma_device_config_init(ma_device_type_playback);
            config.playback.pDeviceID = &info.info.id;
            config.playback.format = ma_format_s16;
            config.playback.channels = CHANNELS;
            config.sampleRate = SAMPLE_RATE;
            config.dataCallback = callback;
            
            ECS::add_component(m_device, CMADevice());
            auto &device = ECS::get_component<CMADevice>(m_device);

            if (ma_device_init(context, &config, &device.device) != MA_SUCCESS) {
                ma_device_uninit(&device.device);
                FATAL("An error occured while initializing device \"{}\"", info.info.name);
            }

            if (is_playing) {
                assert(ma_device_start(&device.device) == MA_SUCCESS);
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
