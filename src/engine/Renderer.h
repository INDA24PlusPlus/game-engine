#ifndef _RENDERER_H
#define _RENDERER_H

#include <glm/glm.hpp>

#include "Camera.h"
#include "core.h"
#include "graphics/Image.h"
#include "graphics/Pipeline.h"
#include "graphics/Sampler.h"
#include "scene/Node.h"
#include "scene/Scene.h"

namespace engine {

class Renderer {
   public:
    friend class Scene;
    typedef void *(*LoadProc)(const char *name);

    void init(LoadProc load_proc);
    void make_resources_for_scene(const loader::AssetFileData &scene);

    // Will go away.
    void set_texture_filtering_level(Scene& scene, f32 level);
    f32 get_max_texture_filtering_level() const;

    void clear();
    void begin_pass(const Scene &scene, const Camera &camera, u32 width, u32 height);
    void end_pass();
    void draw_mesh(u32 mesh_handle, const glm::mat4 &transform);
    void draw_hierarchy(const Scene &scene, const NodeHierarchy &hierarchy);

    // Temp
    void update_light_positions(u32 index, glm::vec4 pos);

   private:
    struct GeneratedImages {
        Image env_map;
        Image brdf_lut;
        Image irradiance_map;
        Image prefiltered_cubemap;
    };

    u32 load_shader(const char *path, u32 shader_type);
    void create_textures(const Scene &scene);
    void create_ubos();

    // Things we might want to move to a offline baking process.
    void generate_offline_content();
    void generate_brdf_lut(Image &brdf_lut);
    void generate_cubemap_from_equirectangular_new(const Image &eq_map,
                                                   const Sampler &eq_map_sampler,
                                                   ImageInfo::Format cubemap_format, Image &result);

    enum class PrefilterDistribution : u32 {
        lambertian = 0,
        GGX = 1,
    };

    void prefilter_cubemap(const Image &env_map, const Image &result, u32 sample_count,
                           PrefilterDistribution dist);
    void prefilter_env_map(const Image &env_map, Image &result);
    void draw_skybox();
    void create_skybox();
    void draw_node(const NodeHierarchy &hierarchy, u32 node_index,
                   const glm::mat4 &parent_transform);

    bool m_scene_loaded;
    bool m_pass_in_progress;

    f32 m_max_texture_filtering;
    Sampler m_default_sampler;

    struct Pass {
        const Scene *scene;
        glm::mat4 projection_matrix;
        glm::mat4 view_matrix;
        glm::vec3 camera_pos;
    };

    struct Skybox {
        Pipeline pipeline;
        Image skybox_image;
    };

    struct UBOMatrices {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };

    struct GPUMaterial {
        glm::vec4 base_color_factor;
        glm::vec4 metallic_roughness_normal_occlusion;
        glm::vec3 camera_pos;
        u32 flags;
        glm::vec3 emissive_factor;
        f32 pad;
    };

    Pass m_curr_pass;

    u32 m_ubo_material_handle;
    u32 m_ubo_matrices_handle;
    u32 m_ubo_light_positions;

    Skybox m_skybox;
    GeneratedImages m_offline_images;

    Pipeline m_pbr_pipeline;
};

}  // namespace engine

#endif
