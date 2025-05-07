#pragma once
#include <miniaudio.h>

#include "engine/ecs/entity.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"

#include "engine/audio/source/source.h"
#include "engine/utils/logging.h"
#include "engine/audio/listener/device.h"

class CMAContext : public Component<CMAContext> {
    public:
        ma_context context;
};

class Audio : public Resource<Audio> {
    private:
        Entity m_context;
        Entity m_device;
    public:
        Audio();
        Audio(ECS &ecs) {
            ecs.register_component<CMAContext>();
            ecs.register_component<CAudioSource>();
            ecs.register_component<CAudioDevice>();
            DEBUG("Registered components");

            ecs.add_component(m_context, CMAContext());
            DEBUG("Added context");
            auto context = ecs.get_component<CMAContext>(m_context);
            DEBUG("Retrieved context");

            if (ma_context_init(NULL, 0, NULL, &context.context) != MA_SUCCESS) {
                FATAL("Unable to initialize miniaudio context");
            }

            DEBUG("Initialized context");

            ma_device_info default_device = get_default_device(get_available_devices(&context.context));
            ecs.add_component(m_device, CAudioDevice(ecs, &context.context, default_device));
            DEBUG("Added device");
        }

        void set_device(ECS &ecs, ma_device_info device_info) {
            auto context = ecs.get_component<CMAContext>(m_context);

            // Handle old device
            ecs.get_component<CAudioDevice>(m_device).deinit(ecs);
            ecs.remove_component<CAudioDevice>(m_device);

            ecs.add_component(m_device, CAudioDevice(ecs, &context.context, device_info));
        }

        void add_source() {

        }
};

