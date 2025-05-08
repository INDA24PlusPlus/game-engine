#pragma once
#include <miniaudio.h>

#include "engine/Camera.h"
#include "engine/audio/listener/listener.h"
#include "engine/ecs/entity.hpp"
#include "engine/ecs/entityarray.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"

#include "engine/audio/source/source.h"
#include "engine/utils/logging.h"
#include "engine/audio/listener/device.h"

class SAudioManager : public System<SAudioManager> {
    private:
        AudioListener listener;
        Entity m_sources = ECS::create_entity();
        u32 active_sources = 0;
    public:
        SAudioManager() {
            ECS::register_component<CAudioSource>();

            queries[0] = Query(CAudioSource::get_id());
            query_count = 1;
        }

        void add_source(CAudioSource &source) {
            ECS::add_component(m_sources, source);
            active_sources += 1;
        }

        void play_sources(SOUND_BUF_TYPE * output, engine::Camera &camera, u32 frame_count) {
            memset(output, 0, frame_count * CHANNELS);

            auto * entities = get_query(0)->get_entities();
            Iterator it = {.next = 0};
            Entity entity;

            float scale = 1.0f / active_sources;
            active_sources = 0;

            glm::mat4 view_mat = camera.get_view_matrix();
            listener.set_position(glm::vec3(view_mat[3][0], view_mat[3][1], view_mat[3][2]));
            listener.set_up(glm::vec3(view_mat[1][0], view_mat[1][1], view_mat[1][2]));
            listener.set_forward(glm::vec3(view_mat[2][0], view_mat[2][1], view_mat[2][2]));

            while (entities->next(it, entity)) {
                CAudioSource &source = ECS::get_component<CAudioSource>(entity);
                source.play(listener, output, frame_count, scale);
                active_sources += source.get_state() == SoundState::Playing;
            }
        }
};

static void data_callback(ma_device * pDevice, void * pOutput, const void * _, ma_uint32 frame_count);

class RAudio : public Resource<RAudio> {
    private:
        ma_context context;

        CAudioDevice device;
        engine::Camera &camera;
        SAudioManager * manager = ECS::register_system<SAudioManager>();
    public:
        RAudio(engine::Camera &camera) 
            : camera(camera)
        {
            if (ma_context_init(NULL, 0, NULL, &this->context) != MA_SUCCESS) {
                FATAL("Unable to initialize miniaudio context");
            }

            ma_device_info default_device = get_default_device(get_available_devices(&this->context));
            this->device = CAudioDevice(&this->context, default_device, &data_callback);
        }

        void set_device(ma_device_info device_info) {
            this->device.deinit();
            this->device = CAudioDevice(&this->context, device_info, &data_callback);
        }

        static void add_source(CAudioSource source) {
            ECS::get_resource<RAudio>()->manager->add_source(source);
        }

        void play_audio_callback(SOUND_BUF_TYPE * output, u32 frame_count) {
            if (!this->device.is_playing) {
                return;
            }

            manager->play_sources(output, this->camera, frame_count);
        }
};

static void data_callback(ma_device * _device, void * pOutput, const void * _input, ma_uint32 frame_count) {
    RAudio * audio = ECS::get_resource<RAudio>();

    if (audio != nullptr) {
        audio->play_audio_callback((SOUND_BUF_TYPE *) pOutput, frame_count);
    }
}
