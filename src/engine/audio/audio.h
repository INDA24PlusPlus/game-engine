#pragma once
#include <miniaudio.h>

#include "engine/ecs/entity.hpp"
#include "engine/ecs/entityarray.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"

#include "engine/audio/source/source.h"
#include "engine/utils/logging.h"
#include "engine/audio/listener/device.h"
#include "engine/utils/timer.h"

class CMAContext : public Component<CMAContext> {
    public:
        ma_context context;
};

class SAudioManager : public System<SAudioManager> {
    private:
        Entity m_sources = ECS::create_entity();
        u32 active_sources = 0;
    public:
        SAudioManager() {
            queries[0] = Query(CAudioSource::get_id());
            query_count = 1;
        }

        void add_source(CAudioSource source) {
            ECS::add_component(m_sources, source);
            active_sources += 1;
        }

        void play_sources(SOUND_BUF_TYPE * output, CAudioDevice &device, u32 frame_count) {
            if (active_sources == 0) {
                return;
            }

            auto * entities = get_query(0)->get_entities();
            Iterator it = {.next = 0};
            Entity entity;

            float scale = 1.0f / active_sources;

            while (entities->next(it, entity)) {
                CAudioSource &source = ECS::get_component<CAudioSource>(entity);
                source.play(output, frame_count, scale);
            }
        }
};

static void data_callback(ma_device * pDevice, void * pOutput, const void * _, ma_uint32 frame_count);

class RAudio : public Resource<RAudio> {
    private:
        Entity m_context = ECS::create_entity();

        CAudioDevice device;
        SAudioManager * manager = ECS::register_system<SAudioManager>();
    public:
        RAudio() {
            ECS::register_component<CMAContext>();
            ECS::add_component(m_context, CMAContext());
            auto &context = ECS::get_component<CMAContext>(m_context);

            if (ma_context_init(NULL, 0, NULL, &context.context) != MA_SUCCESS) {
                FATAL("Unable to initialize miniaudio context");
            }

            ma_device_info default_device = get_default_device(get_available_devices(&context.context));
            device = CAudioDevice(&context.context, default_device, &data_callback);
        }

        void set_device(ma_device_info device_info) {
            device.deinit();

            auto &context = ECS::get_component<CMAContext>(m_context);
            device = CAudioDevice(&context.context, device_info, &data_callback);
        }

        static void add_source(CAudioSource source) {
            RAudio * audio = ECS::get_resource<RAudio>();
            audio->manager->add_source(source);
        }

        void play_audio_callback(SOUND_BUF_TYPE * output, u32 frame_count) {
            if (!device.is_playing) {
                return;
            }

            manager->play_sources(output, device, frame_count);
        }
};

static void data_callback(ma_device * _device, void * pOutput, const void * _input, ma_uint32 frame_count) {
    RAudio * audio = ECS::get_resource<RAudio>();

    if (audio != nullptr) {
        audio->play_audio_callback((SOUND_BUF_TYPE *) pOutput, frame_count);
    }
}
