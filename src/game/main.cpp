#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <print>

#include "engine/Camera.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/core.h"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/entityarray.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/signature.hpp"
#include "engine/utils/logging.h"

#include "gui.h"
#include "state.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

class CMesh : public Component<CMesh> {
    public:
        engine::MeshHandle m_mesh;
        CMesh(engine::MeshHandle mesh) : m_mesh(mesh) {
        }
        CMesh() {
        }
};

class CVelocity : public Component<CVelocity> {
    public:
        glm::vec3 m_velocity;
        CVelocity() : m_velocity(0.0f) {
        }
};


class RInput : public Resource<RInput> {
    public:
        engine::Input m_input;
        RInput(GLFWwindow *window) : m_input(window) {
        }
};

class RRenderer : public Resource<RRenderer> {
    public:
        engine::Renderer m_renderer;
        RRenderer(engine::Renderer renderer) : m_renderer(renderer) {
        }
};

class RScene : public Resource<RScene> {
    public:
        engine::Scene m_scene;
        engine::Camera m_camera;
        GLFWwindow *m_window;
        RScene(engine::Scene scene, engine::Camera camera, GLFWwindow *window)
            : m_scene(scene), m_camera(camera), m_window(window) {
        }
};

class SRenderer : public System<SRenderer> {
    public:
        void update(ECS& ecs) {
            auto renderer = &ecs.get_resource<RRenderer>()->m_renderer;
            auto scene = ecs.get_resource<RScene>();
            auto camera = scene->m_camera;
            INFO("Camera position: {}", camera.m_pos.x);
            auto window = &scene->m_window;
            int height;
            int width;
            glfwPollEvents();
            glfwGetFramebufferSize(*window, &width, &height);
            if (width == 0 || height == 0) {
                return;
            }


            glfwPollEvents();
            scene->m_scene.compute_global_node_transforms();
            renderer->clear();
            renderer->begin_pass(camera, 1920, 1080);
            Entity e;
            Iterator it;
            INFO("Drawing meshes");
            int a = 0;
            while (entities.next(it, e)) {
                INFO("Drawing mesh {}", e);
                auto mesh = ecs.get_component<CMesh>(e);
                INFO("IS_VALID: {}", mesh.m_mesh.is_valid());
                renderer->draw_mesh(scene->m_scene, mesh.m_mesh);
                a++;
            }
            INFO("Drawn {} meshes", a);
            renderer->end_pass();
            glfwSwapBuffers(*window);
        }
};

class SMoveCamera : public System<SMoveCamera> {
    public:
        void update(ECS& ecs) {
            auto input = ecs.get_resource<RInput>()->m_input;
            auto scene = ecs.get_resource<RScene>();
            auto camera = &scene->m_camera;
            auto window = scene->m_window;

            if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
                bool mouse_locked = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
                glfwSetInputMode(window, GLFW_CURSOR,
                                 mouse_locked ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            }

            glm::vec3 direction(0.0f);

            if (input.is_key_pressed(GLFW_KEY_W)) direction.z += 1.0f;
            if (input.is_key_pressed(GLFW_KEY_S)) direction.z -= 1.0f;
            if (input.is_key_pressed(GLFW_KEY_A)) direction.x -= 1.0f;
            if (input.is_key_pressed(GLFW_KEY_D)) direction.x += 1.0f;
            INFO("Direction: {}", direction.x);

            if (glm::length(direction) > 0) {
                if (input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
                    camera->move(glm::normalize(direction), 0.016f);
                } else {
                    auto forward = camera->m_orientation * glm::vec3(0, 0, -1);
                    auto right = camera->m_orientation * glm::vec3(1, 0, 0);
                    forward.y = 0;
                    right.y = 0;

                    forward = glm::normalize(forward);
                    right = glm::normalize(right);
                    auto movement = (right * direction.x + forward * direction.z) *
                                    camera->m_speed * 0.016f;
                    scene->m_scene.m_nodes[1].translation += movement;

                    glm::quat target_rotation =
                        glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));

                    scene->m_scene.m_nodes[1].rotation = glm::slerp(
                        scene->m_scene.m_nodes[1].rotation, target_rotation, 0.016f * 8.0f);

                    // camera movement

                    // glm::quat camera_target_rotation = glm::quatLookAt(glm::normalize(movement),
                    // glm::vec3(0, 1, 0)); state.camera.m_orientation =
                    // glm::slerp(state.camera.m_orientation, camera_target_rotation, state.delta_time);
                    // state.camera.m_pos = state.scene.m_nodes[1].translation +
                    // state.camera.m_orientation * glm::vec3(0,0,10);
                    glm::vec3 camera_offset = glm::vec3(-5, 5, -5);
                    glm::vec3 camera_target_position =
                        scene->m_scene.m_nodes[1].translation + camera_offset;
                    camera->m_pos =
                        glm::mix(camera->m_pos, camera_target_position, 0.016f * 5);
                    camera->m_orientation =
                        glm::quatLookAt(glm::normalize(-camera_offset), glm::vec3(0, 1, 0));
                }
            }
            // mouse input

            if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
                glm::vec2 mouse_delta = input.get_mouse_position_delta();
                camera->rotate(-mouse_delta.x * 0.001f, -mouse_delta.y * 0.001f);
            }
            input.update();
        }
};

template <class... Args>
void fatal(std::format_string<Args...> fmt, Args &&...args) {
    ERROR(fmt, std::forward<Args>(args)...);
    exit(1);
}

static void error_callback(int error, const char *description) {
    ERROR("GLFW error with code {}: {}", error, description);
}

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

void main_necs(void) {
    if (!glfwInit()) {
        fatal("Failed to initiliaze glfw");
    }
    INFO("Initialized GLFW");
    glfwSetErrorCallback(error_callback);

    f32 content_x_scale;
    f32 content_y_scale;
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &content_x_scale, &content_y_scale);
    INFO("Monitor content scale is ({}, {})", content_x_scale, content_y_scale);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_NO_ERROR, GLFW_TRUE);
#endif

    f32 content_scale = std::max(content_x_scale, content_y_scale);
    auto window = glfwCreateWindow(1920 / content_scale, 1080 / content_scale, "Skeletal Animation",
                                   nullptr, nullptr);
    if (!window) {
        fatal("Failed to create glfw window");
    }

    glfwMakeContextCurrent(window);

    engine::Input input(window);
    State state;
    state.camera.init(glm::vec3(0.0f, 0.0f, 3.0f), 10.0f);
    state.mouse_locked = true;
    state.sensitivity = 0.001f;

    engine::Renderer renderer((engine::Renderer::LoadProc)glfwGetProcAddress);
    state.scene.load_asset_file("scene_data.bin");
    state.scene.compute_global_node_transforms();
    renderer.make_resources_for_scene(state.scene);

    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gui::init(window, content_scale);

    engine::MeshHandle helmet_mesh = state.scene.mesh_from_name("SciFiHelmet");
    engine::MeshHandle sponza_mesh = state.scene.mesh_from_name("Sponza");

    // Create ECS
    ECS ecs;
    ecs.register_component<CMesh>();
    ecs.register_component<CVelocity>();

    Signature renderer_signature = 0;
    renderer_signature = set_signature(renderer_signature, CMesh::get_id());
    ecs.set_system_signature<SRenderer>(renderer_signature);

    Entity e1 = ecs.create_entity();
    ecs.add_component<CMesh>(e1, CMesh(helmet_mesh));
    Entity e2 = ecs.create_entity();
    ecs.add_component<CMesh>(e2, CMesh(sponza_mesh));

    while (!glfwWindowShouldClose(window)) {
        state.prev_time = state.curr_time;
        state.curr_time = glfwGetTime();
        state.delta_time = state.curr_time - state.prev_time;

        // Update
        glfwPollEvents();
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0) {
            continue;
        }

        // keyboard input
        if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
            state.mouse_locked = !state.mouse_locked;
            glfwSetInputMode(window, GLFW_CURSOR,
                             state.mouse_locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        glm::vec3 direction(0.0f);

        if (input.is_key_pressed(GLFW_KEY_W)) direction.z += 1.0f;
        if (input.is_key_pressed(GLFW_KEY_S)) direction.z -= 1.0f;
        if (input.is_key_pressed(GLFW_KEY_A)) direction.x -= 1.0f;
        if (input.is_key_pressed(GLFW_KEY_D)) direction.x += 1.0f;

        if (glm::length(direction) > 0) {
            if (input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
                state.camera.move(glm::normalize(direction), state.delta_time);
            } else {
                auto forward = state.camera.m_orientation * glm::vec3(0, 0, -1);
                auto right = state.camera.m_orientation * glm::vec3(1, 0, 0);
                forward.y = 0;
                right.y = 0;

                forward = glm::normalize(forward);
                right = glm::normalize(right);
                auto movement = (right * direction.x + forward * direction.z) *
                                state.camera.m_speed * state.delta_time;
                state.scene.m_nodes[1].translation += movement;

                glm::quat target_rotation =
                    glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));

                state.scene.m_nodes[1].rotation = glm::slerp(
                    state.scene.m_nodes[1].rotation, target_rotation, state.delta_time * 8.0f);

                // camera movement

                // glm::quat camera_target_rotation = glm::quatLookAt(glm::normalize(movement),
                // glm::vec3(0, 1, 0)); state.camera.m_orientation =
                // glm::slerp(state.camera.m_orientation, camera_target_rotation, state.delta_time);
                // state.camera.m_pos = state.scene.m_nodes[1].translation +
                // state.camera.m_orientation * glm::vec3(0,0,10);
                glm::vec3 camera_offset = glm::vec3(-5, 5, -5);
                glm::vec3 camera_target_position =
                    state.scene.m_nodes[1].translation + camera_offset;
                state.camera.m_pos =
                    glm::mix(state.camera.m_pos, camera_target_position, state.delta_time * 5);
                state.camera.m_orientation =
                    glm::quatLookAt(glm::normalize(-camera_offset), glm::vec3(0, 1, 0));
            }
        }

        // mouse input
        if (state.mouse_locked) {
            glm::vec2 mouse_delta = input.get_mouse_position_delta();
            state.camera.rotate(-mouse_delta.x * state.sensitivity,
                                -mouse_delta.y * state.sensitivity);
        }

        glfwPollEvents();
        state.scene.compute_global_node_transforms();
        gui::build(state);

        // Draw
        renderer.clear();
        renderer.begin_pass(state.camera, width, height);
        renderer.draw_mesh(state.scene, helmet_mesh);
        renderer.draw_mesh(state.scene, helmet_mesh,
                           glm::translate(glm::mat4(1.0f), glm::vec3(0, 2, 0)));
        renderer.draw_mesh(state.scene, sponza_mesh);
        // for (int i = 0; i < NUM_PLAYERS; i++) {
        //   if (i == state.multiplayer_client.get_id()) {
        //     continue;
        //   }
        //
        //   // printf("%d -> %d: (%f, %f)\n", state.multiplayer_client.get_id(), i,
        //   //        state.multiplayer_client.get_position(i).x,
        //   //        state.multiplayer_client.get_position(i).y);
        //
        //   glm::mat4 online_player_transform;
        //   {
        //     auto T = glm::translate(
        //         glm::mat4(1.0f),
        //         glm::vec3(state.multiplayer_client.get_position(i).x, 0.0f,
        //                   state.multiplayer_client.get_position(i).y));
        //     auto R = glm::mat4_cast(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        //     auto S = glm::scale(glm::mat4(1.0f), player_scale);
        //     online_player_transform = T * R * S;
        //   }
        //
        //   state.renderer.draw_mesh(state.scene, helmet_mesh,
        //                            online_player_transform);
        // }
        renderer.end_pass();

        gui::render();

        glfwSwapBuffers(window);

        input.update();
    }
}

void main_ecs(void) {
    if (!glfwInit()) {
        fatal("Failed to initiliaze glfw");
    }
    INFO("Initialized GLFW");
    glfwSetErrorCallback(error_callback);

    f32 content_x_scale;
    f32 content_y_scale;
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &content_x_scale, &content_y_scale);
    INFO("Monitor content scale is ({}, {})", content_x_scale, content_y_scale);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_NO_ERROR, GLFW_TRUE);
#endif

    f32 content_scale = std::max(content_x_scale, content_y_scale);
    auto window = glfwCreateWindow(1920 / content_scale, 1080 / content_scale, "Skeletal Animation",
                                   nullptr, nullptr);
    if (!window) {
        fatal("Failed to create glfw window");
    }

    glfwMakeContextCurrent(window);

    engine::Input input(window);
    engine::Camera camera;
    camera.init(glm::vec3(0.0f, 0.0f, 3.0f), 10.0f);
    engine::Scene scene;
    scene.load_asset_file("scene_data.bin");
    scene.compute_global_node_transforms();

    engine::Renderer renderer((engine::Renderer::LoadProc)glfwGetProcAddress);
    renderer.make_resources_for_scene(scene);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    engine::MeshHandle helmet_mesh = scene.mesh_from_name("SciFiHelmet");
    engine::MeshHandle sponza_mesh = scene.mesh_from_name("Sponza");

    // Create ECS
    ECS ecs;
    ecs.register_component<CMesh>();
    ecs.register_component<CVelocity>();

    ecs.register_resource(new RInput(window));
    ecs.register_resource(new RRenderer(renderer));
    ecs.register_resource(new RScene(scene, camera, window));
    auto render_system = ecs.register_system<SRenderer>();
    auto move_camera_system = ecs.register_system<SMoveCamera>();
    Signature renderer_signature = 0;
    renderer_signature = set_signature(renderer_signature, CMesh::get_id());
    ecs.set_system_signature<SRenderer>(renderer_signature);

    Entity e1 = ecs.create_entity();
    ecs.add_component<CMesh>(e1, CMesh(helmet_mesh));
    Entity e2 = ecs.create_entity();
    ecs.add_component<CMesh>(e2, CMesh(sponza_mesh));

    while (!glfwWindowShouldClose(window)) {
        // Update
        move_camera_system->update(ecs);
        render_system->update(ecs);
    }
}

int main(int argc, char **argv) {
    main_ecs();
}
