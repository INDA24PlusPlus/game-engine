#include "engine/Camera.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <print>

#include "engine/AssetLoader.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/core.h"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/entity.hpp"
#include "engine/ecs/entityarray.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/signature.hpp"
#include "engine/ecs/system.hpp"
#include "engine/scene/Node.h"
#include "engine/scene/Scene.h"
#include "engine/utils/logging.h"
#include "glm/detail/qualifier.hpp"
#include "glm/ext/quaternion_common.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "state.h"
#include "settings.h"
#include "world_gen/map.h"

class RDeltaTime : public Resource<RDeltaTime> {
public:
    f32 delta_time;
    f32 prev_time;
    RDeltaTime(f32 current_time): delta_time(0), prev_time(current_time) {}
};

class RInput : public Resource<RInput> {
public:
    engine::Input m_input;
    Settings settings;
    RInput(GLFWwindow *window) : m_input(window) {}
};

class RRenderer : public Resource<RRenderer> {
public:
    engine::Renderer m_renderer;
    RRenderer(engine::Renderer renderer) : m_renderer(renderer) {}
};

class RScene : public Resource<RScene> {
public:
    engine::Scene m_scene;
    engine::Camera m_camera;
    engine::NodeHierarchy m_hierarchy;
    GLFWwindow *m_window;
    Entity m_player; // temporary way of getting the player in singleplayer
    RScene(engine::Scene scene, engine::Camera camera, GLFWwindow *window,
            engine::NodeHierarchy hierarchy, Entity player)
        : m_scene(scene), m_camera(camera), m_hierarchy(hierarchy),
            m_window(window), m_player(player) {}
};

class CPlayer : public Component<CPlayer> {
public:
};

class CSpeed : public Component<CSpeed> {
public:
    float speed = 10;
};

class CHealth : public Component<CHealth> {
public:
    float health = 100;

    void take_damage(float damage) {
        health -= damage;
        if (health < 0.0f) {
            health = 0.0f;
        }
    }

    bool is_alive() {
        return health > 0.0f;
    }
};

class CEnemyGhost : public Component<CEnemyGhost> {
public:
    float cooldown = 0.0f;
};

class CLocal : public Component<CLocal> {
public:
};

class CVelocity : public Component<CVelocity> {
public:
    glm::vec3 vel;
};

class CTranslation : public Component<CTranslation> {
public:
    glm::vec3 pos;
    glm::quat rot;
    glm::vec3 scale;
};

class CMesh : public Component<CMesh> {
public:
    engine::NodeHandle m_node;
    CMesh(engine::NodeHandle node) : m_node(node) {}
    CMesh() {}
};

class SDeltaTime : public System<SDeltaTime> {
public:
    SDeltaTime() {}

    void update(ECS &ecs) {
        auto time = ecs.get_resource<RDeltaTime>();
        time->delta_time = glfwGetTime() - time->prev_time;
        time->prev_time = glfwGetTime();
    }
};

class SMove : public System<SMove> {
public:
    SMove() {
        queries[0] = Query(CTranslation::get_id(), CVelocity::get_id());
        query_count = 1;
    }

    void update(ECS &ecs) {
        auto entities = get_query(0)->get_entities();
        Iterator it = {.next = 0};
        Entity e;
        while (entities->next(it, e)) {
            CTranslation &translation = ecs.get_component<CTranslation>(e);
            CVelocity& vel = ecs.get_component<CVelocity>(e);
            translation.pos += vel.vel;
            vel.vel = glm::vec3(0);
        }
    }
};

class SRender : public System<SRender> {
public:
    SRender() {
        queries[0] = Query(CTranslation::get_id(), CMesh::get_id());
        query_count = 1;
    }

    void update(ECS &ecs) {
        auto renderer = &ecs.get_resource<RRenderer>()->m_renderer;
        auto scene = ecs.get_resource<RScene>();
        auto hierarchy = scene->m_hierarchy;
        auto camera = scene->m_camera;
        auto window = scene->m_window;
        int height;
        int width;

        auto entities = get_query(0)->get_entities();
        Iterator it = {.next = 0};
        Entity e;
        while (entities->next(it, e)) {
            auto translation = ecs.get_component<CTranslation>(e);
            auto mesh = ecs.get_component<CMesh>(e);
            hierarchy.m_nodes[mesh.m_node.get_value()].translation = translation.pos;
            hierarchy.m_nodes[mesh.m_node.get_value()].rotation = translation.rot;
            hierarchy.m_nodes[mesh.m_node.get_value()].scale = translation.scale;
        }

        glfwPollEvents();
        glfwGetFramebufferSize(window, &width, &height);

        // Draw
        renderer->clear();
        renderer->begin_pass(scene->m_scene, camera, width, height);
        renderer->draw_hierarchy(scene->m_scene, hierarchy);
        renderer->end_pass();

        renderer->begin_rect_pass();
        renderer->draw_rect({.x = 100, .y = 100, .width = 100, .height = 100},
                            glm::vec4(1.0, 0.0, 0.0, 0.4));
        renderer->end_rect_pass(width, height);

        glfwSwapBuffers(scene->m_window);
    }
};

class SLocalMove : public System<SLocalMove> {
public:
    SLocalMove() {
        queries[0] = Query(CTranslation::get_id(), CVelocity::get_id(), CLocal::get_id());
        query_count = 1;
    }

    void update(ECS &ecs) {
        //INFO("debug");
        auto time = ecs.get_resource<RDeltaTime>();
        auto scene = ecs.get_resource<RScene>();
        engine::Input& input = ecs.get_resource<RInput>()->m_input;
        Settings settings = ecs.get_resource<RInput>()->settings;
        engine::Camera& camera = scene->m_camera;
        auto window = scene->m_window;
        auto local_player = get_query(0)->get_entities()->first();
        CVelocity &local_vel = ecs.get_component<CVelocity>(*local_player);
        float& player_speed = ecs.get_component<CSpeed>(*local_player).speed;
        CTranslation &local_translation =
            ecs.get_component<CTranslation>(*local_player);

        // mouse locking
        bool mouse_locked = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
        if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
            mouse_locked = !mouse_locked;
            glfwSetInputMode(window, GLFW_CURSOR,
                            mouse_locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        // movement input
        glm::vec3 direction(0.0f);

        if (input.is_key_pressed(GLFW_KEY_W))
            direction.z += 1.0f;
        if (input.is_key_pressed(GLFW_KEY_S))
            direction.z -= 1.0f;
        if (input.is_key_pressed(GLFW_KEY_A))
            direction.x -= 1.0f;
        if (input.is_key_pressed(GLFW_KEY_D))
            direction.x += 1.0f;

        // player and camera movement
        if (input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
            if (glm::length(direction) > 0) {
                camera.move(glm::normalize(direction), time->delta_time);
            }
        } else {
            auto forward = camera.m_orientation * glm::vec3(0, 0, -1);
            auto right = camera.m_orientation * glm::vec3(1, 0, 0);
            forward.y = 0;
            right.y = 0;

            forward = glm::normalize(forward);
            right = glm::normalize(right);
            auto velocity = (right * direction.x + forward * direction.z) * player_speed * time->delta_time;;
            local_vel.vel = velocity;

            if (glm::length(direction) > 0) {
                glm::quat target_rotation =
                    glm::quatLookAt(glm::normalize(velocity), glm::vec3(0, 1, 0));
                    
                local_translation.rot =
                    glm::slerp(local_translation.rot, target_rotation, time->delta_time * 8.0f);
            }

            // camera movement
            float camera_distance = 10;
            glm::vec3 camera_target_position = local_translation.pos 
                - camera.m_orientation * glm::vec3(0, 0, -1) * camera_distance;
            camera.m_pos = glm::mix(camera.m_pos, camera_target_position, time->delta_time * 5);
            
        }

        // mouse input
        if (mouse_locked) {
            glm::vec2 mouse_delta = input.get_mouse_position_delta();
            camera.rotate(-mouse_delta.x * 0.00048f * settings.sensitivity,
                                -mouse_delta.y * 0.00048f * settings.sensitivity);
        }

        input.update();
    }
};

class SEnemyGhost : public System<SEnemyGhost> {
    public:
    SEnemyGhost() {
        queries[0] = Query(CTranslation::get_id(), CVelocity::get_id(), CEnemyGhost::get_id());
        query_count = 1;
    }

    void update(ECS &ecs) {
        auto time = ecs.get_resource<RDeltaTime>();
        auto scene = ecs.get_resource<RScene>();
        
        auto entities = get_query(0)->get_entities();
        Iterator it = {.next = 0};
        Entity e;
        while (entities->next(it, e)) {
            CTranslation &translation = ecs.get_component<CTranslation>(e);
            CVelocity& vel = ecs.get_component<CVelocity>(e);
            CEnemyGhost& ghost = ecs.get_component<CEnemyGhost>(e);
            float& speed = ecs.get_component<CSpeed>(e).speed;
            //float& health = ecs.get_component<CHealth>(e).health;
            
            // TODO: Get nearest player instead of only player in single player
            Entity& nearest_player = scene->m_player;
            CTranslation& nearest_player_translation = ecs.get_component<CTranslation>(nearest_player);
            CHealth& nearest_player_health = ecs.get_component<CHealth>(nearest_player);
            
            
            // update enemy
            if (ghost.cooldown > 0) {
                ghost.cooldown -= time->delta_time;
                //INFO("debug {}", ghost.cooldown);
                continue;
            }
            glm::vec3 enemy_target_look = nearest_player_translation.pos - translation.pos;
            enemy_target_look.y = 0;
        
            float player_distance = glm::length(enemy_target_look);
        
            if (player_distance > 2.0f) {
                // Rotate
                enemy_target_look = glm::normalize(enemy_target_look);
                glm::quat target_rotation = glm::quatLookAt(enemy_target_look, glm::vec3(0, 1, 0));
                translation.rot = glm::slerp(translation.rot, target_rotation, time->delta_time * 2.0f);
                
                // Move in facing direction
                glm::vec3 forward = translation.rot * glm::vec3(0, 0, -1);
                vel.vel = forward * speed * time->delta_time;
        
                // hover effect sine function
                translation.pos.y = glm::sin(time->prev_time * speed) * 0.3;
            } else {
                // Damage player
                nearest_player_health.take_damage(20);
                ghost.cooldown = 1.0;
            }
        }
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

static void gen_world(engine::NodeHierarchy &hierarchy, engine::Scene &scene, engine::NodeHandle &root) {
    Map map(100, 15, 5, 10, 1337);

    engine::NodeHandle map_node = hierarchy.add_node(
        {
            .name = "Map",
            .rotation = glm::quat(1, 0, 0, 0),
            .scale = glm::vec3(5, 1, 5),
        },
        root);

    engine::MeshHandle cube_mesh = scene.mesh_by_name("Cube");

    for (size_t i = 0; i < map.grid.size(); ++i) {
        for (size_t j = 0; j < map.grid[i].size(); ++j) {
            if (map.grid[i][j] != -1) {
                hierarchy.add_node(
                    {
                        .kind = engine::Node::Kind::mesh,
                        .name = std::format("cube_{}_{}", i, j),
                        .rotation = glm::quat(1, 0, 0, 0),
                        .translation = glm::vec3(map.grid.size() * -0.5 + (f32)i, -2.5,
                                                map.grid[i].size() * -0.5 + (f32)j),
                        .scale = glm::vec3(1),
                        .mesh_index = cube_mesh.get_value(),
                    },
                    map_node);
            }
        }
    }
}

int main(void) {
    if (!glfwInit()) {
        fatal("Failed to initiliaze glfw");
    }
    INFO("Initialized GLFW");
    glfwSetErrorCallback(error_callback);

    f32 content_x_scale;
    f32 content_y_scale;
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &content_x_scale,
                                &content_y_scale);
    INFO("Monitor content scale is ({}, {})", content_x_scale, content_y_scale);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 8);
#ifndef NDEBUG
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_NO_ERROR, GLFW_TRUE);
#endif

    f32 content_scale = std::max(content_x_scale, content_y_scale);
    auto window = glfwCreateWindow(1920 / content_scale, 1080 / content_scale,
                                "Skeletal Animation", nullptr, nullptr);
    if (!window) {
        fatal("Failed to create glfw window");
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Create ECS
    ECS ecs = ECS();

    // Register resources
    INFO("Create resources");
    engine::Scene scene;
    engine::Input input(window);
    engine::NodeHierarchy hierarchy;

    engine::NodeHandle root_node = hierarchy.add_root_node({
        .name = "Game",
        .rotation = glm::quat(1, 0, 0, 0),
        .scale = glm::vec3(1),
    });

    engine::Camera camera;
    camera.init(glm::vec3(0.0f, 0.0f, 3.0f), 10.0f);

    engine::Renderer renderer;
    renderer.init((engine::Renderer::LoadProc)glfwGetProcAddress);
    {
        auto data = engine::loader::load_asset_file("scene_data.bin");
        scene.init(data);
        renderer.make_resources_for_scene(data);
    }

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gen_world(hierarchy, scene, root_node);

    // Register componets
    INFO("Create componets");
    ecs.register_component<CPlayer>();
    ecs.register_component<CSpeed>();
    ecs.register_component<CHealth>();
    ecs.register_component<CEnemyGhost>();
    ecs.register_component<CTranslation>();
    ecs.register_component<CVelocity>();
    ecs.register_component<CMesh>();

    // Register systems
    INFO("Create systems");
    auto move_system = ecs.register_system<SMove>();
    auto render_system = ecs.register_system<SRender>();
    auto local_move_system = ecs.register_system<SLocalMove>();
    auto delta_time_system = ecs.register_system<SDeltaTime>();
    auto enemy_ghost_system = ecs.register_system<SEnemyGhost>();

    // Create local player
    INFO("Create player");
    Entity player = ecs.create_entity();
    ecs.add_component<CPlayer>(player, CPlayer());
    ecs.add_component<CSpeed>(player, CSpeed());
    ecs.add_component<CHealth>(player, CHealth());
    ecs.add_component<CTranslation>(
        player, CTranslation{.pos = glm::vec3(0.0f),
                            .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                            .scale = glm::vec3(1.0f)});
    ecs.add_component<CVelocity>(player, CVelocity{.vel = glm::vec3(0.0f)});

    auto player_prefab = scene.prefab_by_name("Player");
    engine::NodeHandle player_node =
        hierarchy.instantiate_prefab(scene, player_prefab, engine::NodeHandle(0));
    
    ecs.add_component<CMesh>(player, CMesh(player_node));
    
    // Create ghosts
    auto ghost_prefab = scene.prefab_by_name("Ghost");
    for (int i = 0; i < 2; i++) {
        Entity ghost = ecs.create_entity();
        engine::NodeHandle ghost_node =
            hierarchy.instantiate_prefab(scene, ghost_prefab, engine::NodeHandle(0));
        ecs.add_component<CEnemyGhost>(ghost, CEnemyGhost{.cooldown = 0.0f});
        ecs.add_component<CSpeed>(ghost, CSpeed{.speed = (float)(3 + i)});
        ecs.add_component<CHealth>(ghost, CHealth());
        ecs.add_component<CTranslation>(ghost, CTranslation{
            .pos = glm::vec3(10.0f, 0.0f, i * 10.0f),
            .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = glm::vec3(1.0f),
        });
        ecs.add_component<CVelocity>(ghost, CVelocity());
        ecs.add_component<CMesh>(ghost, CMesh(ghost_node));
    }

    // Register resources
    ecs.register_resource(new RInput(window));
    ecs.register_resource(new RRenderer(renderer));
    ecs.register_resource(new RScene(scene, camera, window, hierarchy, player));
    ecs.register_resource(new RDeltaTime(glfwGetTime()));

    INFO("Begin game loop");
    while (!glfwWindowShouldClose(window)) {
        // Update
        delta_time_system->update(ecs); // updates delta_time
        local_move_system->update(ecs); // movement and camera control over client/local player
        enemy_ghost_system->update(ecs); // enemy ghost movement and stuff
        move_system->update(ecs);       // moves entities with velocity
        render_system->update(ecs);     // handle rendering
    }
}
