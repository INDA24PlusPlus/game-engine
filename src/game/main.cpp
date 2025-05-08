#include "engine/Camera.h"
#include <cstring>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <print>

#include "engine/AssetLoader.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/audio/audio.h"
#include "engine/audio/source/source.h"
#include "engine/audio/source/wav.h"
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

inline glm::vec4 from_hex(const char* h) {
    unsigned int r, g, b, a = 255;
    if (std::strlen(h) == 7)
        std::sscanf(h, "#%02x%02x%02x", &r, &g, &b);
    else
        std::sscanf(h, "#%02x%02x%02x%02x", &r, &g, &b, &a);
    return glm::vec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
}

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

    void update() {
        auto time = ECS::get_resource<RDeltaTime>();
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

    void update() {
        auto entities = get_query(0)->get_entities();
        Iterator it = {.next = 0};
        Entity e;
        while (entities->next(it, e)) {
            CTranslation &translation = ECS::get_component<CTranslation>(e);
            CVelocity& vel = ECS::get_component<CVelocity>(e);
            translation.pos += vel.vel;
            vel.vel = glm::vec3(0);
        }
    }
};

class SUIRender : public System<SUIRender> {
public:
    SUIRender(){}

    void update() {
        auto time = ECS::get_resource<RDeltaTime>();
        auto renderer = &ECS::get_resource<RRenderer>()->m_renderer;
        auto scene = ECS::get_resource<RScene>();
        auto window = scene->m_window;
        
        int height;
        int width;
        glfwGetFramebufferSize(window, &width, &height);

        // DRAW HEALTH BAR

        // TODO: Get nearest player instead of only player in single player
        Entity& nearest_player = scene->m_player;
        CHealth& nearest_player_health = ECS::get_component<CHealth>(nearest_player);

        // Draw
        renderer->begin_rect_pass();
        {
            float hpbar_width = 256.f;
            float hpbar_height = 32.f;
            float margin = 12.f;
            float border_size = 4.f;

            static float smooth_health = nearest_player_health.health;
            static float last_health = nearest_player_health.health;
            static float damage_timer = 0.f;     

            // smooth taken damage indicator bar
            if(nearest_player_health.health < last_health) {
                last_health = nearest_player_health.health;
                damage_timer = 0.2f;
            }
            if(damage_timer > 0.f) {
                damage_timer -= time->delta_time;
                if(damage_timer <= 0.f) {
                    last_health = nearest_player_health.health;
                }
            }else {
                smooth_health = glm::mix(smooth_health, nearest_player_health.health, time->delta_time * 10.f);
            }

            float max_health = 100.f; // temporary

            // damage indicator bar (shows how much damage was taken)
            float damage_bar_width = hpbar_width * (nearest_player_health.health / max_health);
            renderer->draw_rect({ .x = (float)width - hpbar_width - margin, .y = margin, .width = damage_bar_width, .height = hpbar_height}, from_hex("#ff0000a0"));

            // hp bar
            float bar_width = hpbar_width * (smooth_health / max_health);
            renderer->draw_rect({ .x = (float)width - hpbar_width - margin, .y = margin, .width = bar_width, .height = hpbar_height}, from_hex("#ff8519a0"));
            
            // background
            renderer->draw_rect({ .x = (float)width - hpbar_width - margin, .y = margin, .width = hpbar_width, .height = hpbar_height}, from_hex("#000000a0"));

            // border
            renderer->draw_rect({ .x = (float)width - hpbar_width - margin - border_size, .y = margin - border_size, .width = hpbar_width + 2 * border_size, .height = hpbar_height + 2 * border_size}, from_hex("#2a2a2aff"));
        }
        renderer->end_rect_pass(width, height); 
    }
};

class SRender : public System<SRender> {
public:
    SRender() {
        queries[0] = Query(CTranslation::get_id(), CMesh::get_id());
        query_count = 1;
    }

    void update() {
        auto renderer = &ECS::get_resource<RRenderer>()->m_renderer;
        auto scene = ECS::get_resource<RScene>();
        auto hierarchy = scene->m_hierarchy;
        auto camera = scene->m_camera;
        auto window = scene->m_window;
        int height;
        int width;

        auto entities = get_query(0)->get_entities();
        Iterator it = {.next = 0};
        Entity e;
        while (entities->next(it, e)) {
            auto translation = ECS::get_component<CTranslation>(e);
            auto mesh = ECS::get_component<CMesh>(e);
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
    }
};

class SLocalMove : public System<SLocalMove> {
public:
    SLocalMove() {
        queries[0] = Query(CTranslation::get_id(), CVelocity::get_id(), CLocal::get_id());
        query_count = 1;
    }

    void update() {
        //INFO("debug");
        auto time = ECS::get_resource<RDeltaTime>();
        auto scene = ECS::get_resource<RScene>();
        engine::Input& input = ECS::get_resource<RInput>()->m_input;
        Settings settings = ECS::get_resource<RInput>()->settings;
        engine::Camera& camera = scene->m_camera;
        auto window = scene->m_window;
        auto local_player = get_query(0)->get_entities()->first();
        CVelocity &local_vel = ECS::get_component<CVelocity>(*local_player);
        float& player_speed = ECS::get_component<CSpeed>(*local_player).speed;
        CTranslation &local_translation =
            ECS::get_component<CTranslation>(*local_player);

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

    void update() {
        auto time = ECS::get_resource<RDeltaTime>();
        auto scene = ECS::get_resource<RScene>();
        
        auto entities = get_query(0)->get_entities();
        Iterator it = {.next = 0};
        Entity e;
        while (entities->next(it, e)) {
            CTranslation &translation = ECS::get_component<CTranslation>(e);
            CVelocity& vel = ECS::get_component<CVelocity>(e);
            CEnemyGhost& ghost = ECS::get_component<CEnemyGhost>(e);
            float& speed = ECS::get_component<CSpeed>(e).speed;
            //float& health = ECS::get_component<CHealth>(e).health;
            
            // TODO: Get nearest player instead of only player in single player
            Entity& nearest_player = scene->m_player;
            CTranslation& nearest_player_translation = ECS::get_component<CTranslation>(nearest_player);
            CHealth& nearest_player_health = ECS::get_component<CHealth>(nearest_player);
            
            
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
        FATAL("Failed to initiliaze glfw");
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
        FATAL("Failed to create glfw window");
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

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
    ECS::register_component<CPlayer>();
    ECS::register_component<CSpeed>();
    ECS::register_component<CHealth>();
    ECS::register_component<CEnemyGhost>();
    ECS::register_component<CTranslation>();
    ECS::register_component<CVelocity>();
    ECS::register_component<CMesh>();

    // Register systems
    INFO("Create systems");
    auto move_system = ECS::register_system<SMove>();
    auto render_system = ECS::register_system<SRender>();
    auto ui_render_system = ECS::register_system<SUIRender>();
    auto local_move_system = ECS::register_system<SLocalMove>();
    auto delta_time_system = ECS::register_system<SDeltaTime>();
    auto enemy_ghost_system = ECS::register_system<SEnemyGhost>();

    // Create local player
    INFO("Create player");
    Entity player = ECS::create_entity();
    ECS::add_component<CPlayer>(player, CPlayer());
    ECS::add_component<CSpeed>(player, CSpeed());
    ECS::add_component<CHealth>(player, CHealth());
    ECS::add_component<CTranslation>(
        player, CTranslation{.pos = glm::vec3(0.0f),
                            .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                            .scale = glm::vec3(1.0f)});
    ECS::add_component<CVelocity>(player, CVelocity{.vel = glm::vec3(0.0f)});

    auto player_prefab = scene.prefab_by_name("Player");
    engine::NodeHandle player_node =
        hierarchy.instantiate_prefab(scene, player_prefab, engine::NodeHandle(0));
    
    ECS::add_component<CMesh>(player, CMesh(player_node));
    
    // Create ghosts
    auto ghost_prefab = scene.prefab_by_name("Ghost");
    for (int i = 0; i < 2; i++) {
        Entity ghost = ECS::create_entity();
        engine::NodeHandle ghost_node =
            hierarchy.instantiate_prefab(scene, ghost_prefab, engine::NodeHandle(0));
        ECS::add_component<CEnemyGhost>(ghost, CEnemyGhost{.cooldown = 0.0f});
        ECS::add_component<CSpeed>(ghost, CSpeed{.speed = (float)(3 + i)});
        ECS::add_component<CHealth>(ghost, CHealth());
        ECS::add_component<CTranslation>(ghost, CTranslation{
            .pos = glm::vec3(10.0f, 0.0f, i * 10.0f),
            .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = glm::vec3(1.0f),
        });
        ECS::add_component<CVelocity>(ghost, CVelocity());
        ECS::add_component<CMesh>(ghost, CMesh(ghost_node));
    }

    // Register resources
    ECS::register_resource(new RInput(window));
    ECS::register_resource(new RRenderer(renderer));
    ECS::register_resource(new RScene(scene, camera, window, hierarchy, player));
    ECS::register_resource(new RDeltaTime(glfwGetTime()));
    auto * audio = ECS::register_resource(new RAudio());
    DEBUG("Audio registered");

    audio->add_source(CAudioSource("../audio/assets/suzume-ost.wav"));
    DEBUG("Added source");

    INFO("Begin game loop");
    while (!glfwWindowShouldClose(window)) {
        // Update
        delta_time_system->update();     // updates delta_time
        // local_move_system->update();     // movement and camera control over client/local player
        // enemy_ghost_system->update();    // enemy ghost movement and stuff
        move_system->update();           // moves entities with velocity
        render_system->update();         // handle rendering
        ui_render_system->update();      // handle rendering of UI

        glfwSwapBuffers(window);
    }
}
