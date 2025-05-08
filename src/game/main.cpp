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
#include "engine/Game.h"
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
    engine::Input input;
    Settings settings;
    RInput(GLFWwindow *window) : input(window) {}

    // returns a vec3 with xz coordinates for movement keys in settings
    glm::vec3 get_direction() {
        glm::vec3 direction(0.0f);

        if (input.is_key_pressed(settings.key_forward))
            direction.z += 1.0f;
        if (input.is_key_pressed(settings.key_backward))
            direction.z -= 1.0f;
        if (input.is_key_pressed(settings.key_left))
            direction.x -= 1.0f;
        if (input.is_key_pressed(settings.key_right))
            direction.x += 1.0f;
        
        return direction;
    }
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
    float attack_cooldown = 0.0f;
    float attack_damage = 50.0f;
    float attack_range_squared = 25.0f;
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
    
    void move_over_time(glm::vec3 move, float time) {
        move_over_time_vel = move / time;
        move_over_time_time = time;
    }

    glm::vec3 move_over_time_vel;
    float move_over_time_time = 0.0;
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

class SInput : public System<SInput> {
public:
    SInput() {}

    void update(ECS &ecs) {
        auto input = ecs.get_resource<RInput>();
        input->input.update();
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
            if (vel.move_over_time_time > 0.0f) {
                float delta_time = ecs.get_resource<RDeltaTime>()->delta_time;
                translation.pos += vel.move_over_time_vel * delta_time;
                vel.move_over_time_time -= delta_time;
            }
        }
    }
};

class SUIRender : public System<SUIRender> {
public:
    SUIRender(){}

    void update(ECS &ecs) {
        auto time = ecs.get_resource<RDeltaTime>();
        auto renderer = &ecs.get_resource<RRenderer>()->m_renderer;
        auto scene = ecs.get_resource<RScene>();
        auto window = scene->m_window;
        
        int height;
        int width;
        glfwGetFramebufferSize(window, &width, &height);

        // DRAW HEALTH BAR

        // TODO: Get nearest player instead of only player in single player
        Entity& nearest_player = scene->m_player;
        CHealth& nearest_player_health = ecs.get_component<CHealth>(nearest_player);

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
    }
};

class SCamera : public System<SCamera> {
public:
    SCamera() {
        queries[0] = Query(CTranslation::get_id(), CLocal::get_id(), CPlayer::get_id());
        query_count = 1;
    }

    void update(ECS &ecs) {
        float delta_time = ecs.get_resource<RDeltaTime>()->delta_time;
        auto scene = ecs.get_resource<RScene>();
        auto window = scene->m_window;
        RInput* input = ecs.get_resource<RInput>();;
        
        engine::Camera& camera = scene->m_camera;
        
        // mouse locking
        bool mouse_locked = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
        if (input->input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
            mouse_locked = !mouse_locked;
            glfwSetInputMode(window, GLFW_CURSOR,
                            mouse_locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        // camera rotation mouse input
        if (mouse_locked) {
            glm::vec2 mouse_delta = input->input.get_mouse_position_delta();
            camera.rotate(-mouse_delta.x * 0.00048f * input->settings.sensitivity,
                                -mouse_delta.y * 0.00048f * input->settings.sensitivity);
        }

        // camera movement
        if (input->input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
            // free cam movement
            // movement input
            glm::vec3 direction = input->get_direction();

            if (glm::dot(direction, direction) > 0) {
                camera.move(glm::normalize(direction), delta_time);
            }
        } else { // this query could be changed to something like CCameraFollow instead of being hardcoded to local players
            // follow player cam
            // get players
            auto players = get_query(0)->get_entities();
            glm::vec3 mean_player_position = glm::vec3(0.0f, 0.0f, 0.0f);
            Iterator it = {.next = 0};
            Entity e;
            int player_count = 0;
            while (players->next(it, e)) {
                mean_player_position += ecs.get_component<CTranslation>(e).pos;
                player_count++;
            }
            mean_player_position /= player_count;

            if (player_count > 0)
            {
                float camera_distance = 10;
                
                glm::vec3 camera_target_position = mean_player_position
                    - camera.m_orientation * glm::vec3(0, 0, -1) * camera_distance;
                camera.m_pos = glm::mix(camera.m_pos, camera_target_position, delta_time * 5);
            }
        }
    }
};

class SPlayerController : public System<SPlayerController> {
public:
    SPlayerController() {
        queries[0] = Query(CTranslation::get_id(), CVelocity::get_id(), CLocal::get_id(), CPlayer::get_id());
        queries[1] = Query(CTranslation::get_id(), CVelocity::get_id(), CEnemyGhost::get_id(), CHealth::get_id());
        query_count = 2;
    }

    void update(ECS &ecs) {
        float delta_time = ecs.get_resource<RDeltaTime>()->delta_time;
        auto scene = ecs.get_resource<RScene>();
        engine::Camera& camera = scene->m_camera;

        RInput& input = *ecs.get_resource<RInput>();
        //Settings settings = ecs.get_resource<RInput>()->settings;

        // Get local player from query
        Entity player;
        {
            auto player_ptr = get_query(0)->get_entities()->first();
            if (player_ptr == nullptr) {
                ERROR("NO PLAYER FOUND!!");
                return;
            }
            player = *player_ptr;
        }


        CVelocity& velocity = ecs.get_component<CVelocity>(player);
        CPlayer& c_player = ecs.get_component<CPlayer>(player);
        float& player_speed = ecs.get_component<CSpeed>(player).speed;
        CTranslation &translation =
            ecs.get_component<CTranslation>(player);
        

        // movement input
        glm::vec3 direction = input.get_direction();

        // player movement
        if (!input.input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)){
            auto forward = camera.m_orientation * glm::vec3(0, 0, -1);
            auto right = camera.m_orientation * glm::vec3(1, 0, 0);
            forward.y = 0;
            right.y = 0;

            forward = glm::normalize(forward);
            right = glm::normalize(right);
            velocity.vel = (right * direction.x + forward * direction.z) * player_speed * delta_time;;

            if (glm::length(direction) > 0) {
                glm::quat target_rotation =
                    glm::quatLookAt(glm::normalize(velocity.vel), glm::vec3(0, 1, 0));
                    
                translation.rot =
                    glm::slerp(translation.rot, target_rotation, delta_time * 8.0f);
            }
        }

        if (c_player.attack_cooldown <= 0.0f) {
            if (input.input.is_key_pressed(input.settings.key_attack)) {
                c_player.attack_cooldown += 0.3f;
                // TODO: sword animation from 45 degrees to the right of player rotation 
                //       to 45 degrees to the left of player rotation
                //       and adjust range

                auto ghosts = get_query(1)->get_entities();
                Iterator it = {.next = 0};
                Entity ghost;
                while (ghosts->next(it, ghost)) {
                    CTranslation& ghost_translation = ecs.get_component<CTranslation>(ghost);
                    
                    glm::vec3 diff_player_ghost = ghost_translation.pos - translation.pos;
                    diff_player_ghost.y = 0.0f;

                    float distance_player_ghost_squared = glm::dot(diff_player_ghost, diff_player_ghost);
                    if (distance_player_ghost_squared <= c_player.attack_range_squared) {
                        // ghost was in range
                        // check angle
                        glm::vec3 forward_direction = translation.rot * glm::vec3(0, 0, -1);
                        glm::vec3 direction_to_ghost = glm::normalize(diff_player_ghost - forward_direction * 0.8f);
                        float cosine_angle = glm::dot(direction_to_ghost, forward_direction);
                        if (cosine_angle > 0.7f) {
                            // ghost was hit!
                            ecs.get_component<CHealth>(ghost).take_damage(c_player.attack_damage);
                            CVelocity& ghost_vel = ecs.get_component<CVelocity>(ghost);
                            ghost_vel.move_over_time(forward_direction * 10.0f, 0.3f);
                        }
                    }
                }
            }
        } else {
            c_player.attack_cooldown -= delta_time;
        }
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
            CHealth& health = ecs.get_component<CHealth>(e);
            if (!health.is_alive()) {
                // ecs.destroy_entity(e); this was problem
                translation.pos.y = -3.0;
                continue;
            }
            
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
    auto handle = game::init(1920, 1080, "Game");

    // Create ECS
    ECS ecs = ECS();

    // Register resources
    INFO("Create resources");
    engine::Scene scene;
    engine::Input input(handle.window);
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
    glfwGetFramebufferSize(handle.window, &width, &height);
    glViewport(0, 0, width, height);
    glfwSetInputMode(handle.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
    auto ui_render_system = ecs.register_system<SUIRender>();
    auto player_controller_system = ecs.register_system<SPlayerController>();
    auto camera_system = ecs.register_system<SCamera>();
    auto delta_time_system = ecs.register_system<SDeltaTime>();
    auto enemy_ghost_system = ecs.register_system<SEnemyGhost>();
    auto input_system = ecs.register_system<SInput>();

    // Create local player
    INFO("Create player");
    Entity player = ecs.create_entity();
    ecs.add_component<CPlayer>(player, CPlayer());
    ecs.add_component<CLocal>(player, CLocal());
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
        ecs.add_component<CEnemyGhost>(ghost, CEnemyGhost({.cooldown = 0.0f}));
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
    ecs.register_resource(new RInput(handle.window));
    ecs.register_resource(new RRenderer(renderer));
    ecs.register_resource(new RScene(scene, camera, handle.window, hierarchy, player));
    ecs.register_resource(new RDeltaTime(glfwGetTime()));

    INFO("Begin game loop");
    
    while (game::should_run(handle)) {
        // Update
        delta_time_system->update(ecs);         // updates delta_time
        player_controller_system->update(ecs);  // player controller for local player(movement and stuff)
        camera_system->update(ecs);             // camera control
        enemy_ghost_system->update(ecs);        // enemy ghost movement and stuff
        move_system->update(ecs);               // moves entities with velocity
        
        input_system->update(ecs);              // stores this frames keys as previous frames keys (do this last, before draw)

        // Draw
        render_system->update(ecs);             // handle rendering
        ui_render_system->update(ecs);          // handle rendering of UI


       game::end_frame(handle);
    }
}
