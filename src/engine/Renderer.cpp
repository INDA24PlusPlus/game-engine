#include "Renderer.h"

#include <glad/glad.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <string>

#include "AssetLoader.h"
#include "glm/fwd.hpp"
#include "graphics/Image.h"
#include "graphics/Pipeline.h"
#include "graphics/Sampler.h"
#include "scene/Node.h"
#include "utils/logging.h"

// Thins we can do to improve performance:
// Decrease size of vertices (half precision etc)
// Pack textures better instead of using RGBA BC7 for *most color data*

// Super temp
#define STB_IMAGE_IMPLEMENTATION
#include "../../tools/asset_processor/build/_deps/tinygltf-src/stb_image.h"

namespace engine {

static void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
                                   GLsizei length, const char *message, const void *userParam) {
    (void)source;
    (void)id;
    (void)severity;
    (void)length;
    (void)userParam;
    (void)type;
    // ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;
    ERROR("OpenGL debug output: {}", message);
}

void Renderer::init(LoadProc load_proc) {
    INFO("Intiliazing renderer");
    m_scene_loaded = false;
    m_pass_in_progress = false;

    if (!gladLoadGLLoader((GLADloadproc)load_proc)) {
        ERROR("Failed to load OpenGL function pointers");
        exit(1);
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &m_max_texture_filtering);

    m_default_sampler.init(Sampler::Filter::linear, Sampler::Filter::linear,
                           Sampler::MipmapMode::linear, Sampler::AddressMode::clamp_to_edge,
                           Sampler::AddressMode::clamp_to_edge, Sampler::AddressMode::clamp_to_edge,
                           m_max_texture_filtering);

    create_ubos();
    generate_offline_content();
    create_skybox();
    INFO("Initialized renderer");
}

f32 Renderer::get_max_texture_filtering_level() const { return m_max_texture_filtering; }

// FIXME: Hack
void Renderer::set_texture_filtering_level(Scene& scene, f32 level) {
    for (size_t i = 0; i < scene.m_samplers.size(); ++i) {
        glSamplerParameterf(scene.m_samplers[i].m_handle, GL_TEXTURE_MAX_ANISOTROPY, level);
    }
}

void Renderer::make_resources_for_scene(const loader::AssetFileData &data) {
    INFO("Creating renderer state for scene data.");
    assert(!m_scene_loaded);
    m_pbr_pipeline.init();

    std::array<VertexAttributeDescriptor, 4> attribs = {
        VertexAttributeDescriptor{.type = VertexAttributeDescriptor::Type::f32,
                                  .count = 4,
                                  .offset = offsetof(Vertex, tangent)},
        VertexAttributeDescriptor{.type = VertexAttributeDescriptor::Type::f32,
                                  .count = 3,
                                  .offset = offsetof(Vertex, pos)},
        VertexAttributeDescriptor{.type = VertexAttributeDescriptor::Type::f32,
                                  .count = 3,
                                  .offset = offsetof(Vertex, normal)},
        VertexAttributeDescriptor{.type = VertexAttributeDescriptor::Type::f32,
                                  .count = 2,
                                  .offset = offsetof(Vertex, uv)},
    };

    std::span vertex_data((u8 *)data.vertices.data(), data.vertices.size() * sizeof(Vertex));
    m_pbr_pipeline.add_vertex_buffer(attribs, sizeof(Vertex), vertex_data);
    m_pbr_pipeline.add_index_buffer(data.indices);

    std::array<u32, 1> specialization_constants = {m_offline_images.env_map.m_info.num_levels};
    m_pbr_pipeline.add_vertex_shader("shaders/SPIRV/basic.vert.spv", {});
    m_pbr_pipeline.add_fragment_shader("shaders/SPIRV/basic.frag.spv", specialization_constants);
    assert(m_pbr_pipeline.m_vshader && "Failed to load PBR vertex shader");
    assert(m_pbr_pipeline.m_fshader && "Failed to load PBR fragment shader");
    m_pbr_pipeline.compile();

    m_scene_loaded = true;
    INFO("Created renderer state for scene object.");
}

void Renderer::clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

void Renderer::begin_pass(const Scene &scene, const Camera &camera, u32 width, u32 height) {
    assert(!m_pass_in_progress);

    m_pbr_pipeline.bind();

    // Bind enviroment textures.
    glBindSampler(5, m_default_sampler.m_handle);
    glBindSampler(6, m_default_sampler.m_handle);
    glBindSampler(7, m_default_sampler.m_handle);

    glBindTextureUnit(5, m_offline_images.brdf_lut.m_handle);
    glBindTextureUnit(6, m_offline_images.irradiance_map.m_handle);
    glBindTextureUnit(7, m_offline_images.prefiltered_cubemap.m_handle);

    glEnable(GL_FRAMEBUFFER_SRGB);

    f32 aspect_ratio = (f32)width / (f32)height;
    UBOMatrices matrices;
    matrices.view = camera.get_view_matrix();
    matrices.projection = glm::perspective(glm::radians(90.0f), aspect_ratio, 0.1f, 1000.0f);

    glNamedBufferSubData(m_ubo_matrices_handle, 0, sizeof(UBOMatrices), &matrices);
    m_curr_pass = {
        .scene = &scene,
        .projection_matrix = matrices.projection,
        .view_matrix = matrices.view,
        .camera_pos = camera.m_pos,
    };
    m_pass_in_progress = true;
}

void Renderer::end_pass() {
    draw_skybox();
    glDisable(GL_FRAMEBUFFER_SRGB);
    assert(m_pass_in_progress);
    m_pass_in_progress = false;
}

void Renderer::draw_mesh(u32 mesh_handle, const glm::mat4 &transform) {
    const auto &scene = *m_curr_pass.scene;

    // FIXME: Do the actual draw calls in end_pass
    const auto &mesh = scene.m_meshes[mesh_handle];
    glNamedBufferSubData(m_ubo_matrices_handle, 0, sizeof(glm::mat4), glm::value_ptr(transform));

    for (size_t i = 0; i < mesh.num_primitives; ++i) {
        const auto &prim = scene.m_primitives[mesh.primitive_index + i];
        const auto &material = scene.m_materials[prim.material_index];

        if ((u32)material.flags & (u32)Material::Flags::has_base_color_texture) {
            const auto &base_color_texture = scene.m_textures[material.base_color_texture];
            glBindSampler(0, scene.m_samplers[base_color_texture.sampler_index].m_handle);
            glBindTextureUnit(0, scene.m_images[base_color_texture.image_index].m_handle);
        }
        if ((u32)material.flags & (u32)Material::Flags::has_metallic_roughness_texture) {
            const auto &metallic_roughness_texture =
                scene.m_textures[material.metallic_roughness_texture];
            glBindSampler(1, scene.m_samplers[metallic_roughness_texture.sampler_index].m_handle);
            glBindTextureUnit(1, scene.m_images[metallic_roughness_texture.image_index].m_handle);
        }
        if ((u32)material.flags & (u32)Material::Flags::has_normal_map) {
            const auto &normal_map = scene.m_textures[material.normal_map];
            glBindSampler(2, scene.m_samplers[normal_map.sampler_index].m_handle);
            glBindTextureUnit(2, scene.m_images[normal_map.image_index].m_handle);
        }
        if ((u32)material.flags & (u32)Material::Flags::has_occlusion_map) {
            const auto &occlusion_map = scene.m_textures[material.occlusion_map];
            glBindSampler(3, scene.m_samplers[occlusion_map.sampler_index].m_handle);
            glBindTextureUnit(3, scene.m_images[occlusion_map.image_index].m_handle);
        }
        if ((u32)material.flags & (u32)Material::Flags::has_emission_map) {
            const auto &emission_map = scene.m_textures[material.emission_map];
            glBindSampler(4, scene.m_samplers[emission_map.sampler_index].m_handle);
            glBindTextureUnit(4, scene.m_images[emission_map.image_index].m_handle);
        }

        // set uniforms for the material.
        GPUMaterial gpu_material = {
            .base_color_factor = material.base_color_factor,
            .metallic_roughness_normal_occlusion =
                glm::vec4(material.metallic_factor, material.roughness_factor,
                          material.normal_map_scale, material.occlusion_strength),
            .camera_pos = m_curr_pass.camera_pos,
            .flags = (u32)material.flags,
            .emissive_factor = material.emission_factor,
        };
        glNamedBufferSubData(m_ubo_material_handle, 0, sizeof(GPUMaterial), &gpu_material);

        auto num_indices = prim.num_indices();
        u64 byte_offset = prim.indices_start;

        glDrawElementsBaseVertex(GL_TRIANGLES, num_indices, prim.index_type, (void *)byte_offset,
                                 prim.base_vertex);
    }
}

void Renderer::update_light_positions(u32 index, glm::vec4 pos) {
    glNamedBufferSubData(m_ubo_light_positions, sizeof(glm::vec4) * index, sizeof(glm::vec4),
                         glm::value_ptr(pos));
}

void Renderer::create_ubos() {
    {
        glCreateBuffers(1, &m_ubo_matrices_handle);
        u32 size = sizeof(UBOMatrices) * 1;
        glNamedBufferData(m_ubo_matrices_handle, size, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ubo_matrices_handle);
    }

    {
        glCreateBuffers(1, &m_ubo_material_handle);
        u32 size = sizeof(GPUMaterial) * 1;
        glNamedBufferData(m_ubo_material_handle, size, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_ubo_material_handle);
    }

    {
        glCreateBuffers(1, &m_ubo_light_positions);
        u32 size = sizeof(glm::vec4) * 5;
        glNamedBufferData(m_ubo_light_positions, size, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_ubo_light_positions);
    }
}

u32 Renderer::load_shader(const char *path, u32 shader_type) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ERROR("Failed to open shader file at {}", path);
        return UINT32_MAX;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();
    const char *source = src.c_str();
    u32 shader_handle = glCreateShaderProgramv(shader_type, 1, &source);

    i32 linked;
    glGetProgramiv(shader_handle, GL_LINK_STATUS, &linked);

    if (!linked) {
        char msg[4096];
        glGetProgramInfoLog(shader_handle, sizeof(msg), nullptr, msg);
        ERROR("Failed to compile shader {} with error: {}", path, msg);
        return UINT32_MAX;
    }
    return shader_handle;
}

void Renderer::draw_skybox() {
    glDepthFunc(GL_LEQUAL);
    m_skybox.pipeline.bind();
    glBindTextureUnit(0, m_skybox.skybox_image.m_handle);

    // Remove translation from the view matrix.
    auto view = glm::mat4(glm::mat3(m_curr_pass.view_matrix));
    glNamedBufferSubData(m_ubo_matrices_handle, sizeof(glm::mat4), sizeof(glm::mat4), &view);

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthFunc(GL_LESS);
}

void Renderer::draw_node(const NodeHierarchy &hierarchy, u32 node_index,
                         const glm::mat4 &parent_transform) {
    const auto &node = hierarchy.m_nodes[node_index];

    auto T = glm::translate(glm::mat4(1.0f), node.translation);
    auto R = glm::mat4_cast(node.rotation);
    auto S = glm::scale(glm::mat4(1.0f), node.scale);
    auto node_transform = T * R * S;
    auto global_transform = parent_transform * node_transform;
    if (node.kind == Node::Kind::mesh) {
        draw_mesh(node.mesh_index, global_transform);
    }

    for (size_t i = 0; i < node.children.size(); i++) {
        draw_node(hierarchy, node.children[i], global_transform);
    }
}

void Renderer::draw_hierarchy(const Scene &scene, const NodeHierarchy &hierarchy) {
    draw_node(hierarchy, 0, glm::mat4(1.0f));
}

// clang-format off
f32 skybox_vertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};
// clang-format on

void Renderer::create_skybox() {
    m_skybox.pipeline.init();

    std::array<VertexAttributeDescriptor, 1> attribs = {VertexAttributeDescriptor{
        .type = VertexAttributeDescriptor::Type::f32, .count = 3, .offset = 0}};

    m_skybox.pipeline.add_vertex_buffer(attribs, 3 * sizeof(f32),
                                        std::span((u8 *)skybox_vertices, 36 * 3 * sizeof(f32)));
    m_skybox.pipeline.add_vertex_shader("shaders/SPIRV/skybox.vert.spv");
    m_skybox.pipeline.add_fragment_shader("shaders/SPIRV/skybox.frag.spv");
    m_skybox.pipeline.compile();
    m_skybox.skybox_image = m_offline_images.env_map;
}

template <typename Func>
static void render_to_cubemap_faces(Pipeline &pipeline, u32 face_size, u32 cubemap, u32 level,
                                    Func f) {
    u32 capture_fbo;
    glCreateFramebuffers(1, &capture_fbo);

    u32 capture_rbo;
    glCreateRenderbuffers(1, &capture_rbo);
    glNamedRenderbufferStorage(capture_rbo, GL_DEPTH_COMPONENT24, face_size, face_size);
    glNamedFramebufferRenderbuffer(capture_fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo);

    glm::mat4 capture_views[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                    glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                    glm::vec3(0.0f, -1.0f, 0.0f))};

    glViewport(0, 0, face_size, face_size);
    glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);

    pipeline.bind();
    for (size_t face = 0; face < 6; ++face) {
        f(face, capture_views[face]);
        glNamedFramebufferTextureLayer(capture_fbo, GL_COLOR_ATTACHMENT0, cubemap, level, face);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &capture_fbo);
    glDeleteRenderbuffers(1, &capture_rbo);
}

void Renderer::generate_offline_content() {
    generate_brdf_lut(m_offline_images.brdf_lut);

    stbi_set_flip_vertically_on_load(true);
    int width, height, num_comps;
    f32 *data = stbi_loadf("assets/monkstown_castle_4k.hdr", &width, &height, &num_comps, 0);
    assert(num_comps == 3);

    if (!data) {
        ERROR("Failed to load HDR image");
        exit(1);
    }
    Image eq_map;
    eq_map.init({
        .format = ImageInfo::Format::RGB32F,
        .width = (u32)width,
        .height = (u32)height,
        .num_levels = 1,
        .num_faces = 1,
    });

    eq_map.upload((u8 *)data);
    stbi_image_free(data);
    generate_cubemap_from_equirectangular_new(eq_map, m_default_sampler, ImageInfo::Format::RGB32F,
                                              m_offline_images.env_map);
    m_offline_images.irradiance_map.init({
        .format = ImageInfo::Format::RGBA32F,
        .width = 128,
        .height = 128,
        .num_levels = 1,
        .num_faces = 6,
        .is_cubemap = true,
    });

    prefilter_cubemap(m_offline_images.env_map, m_offline_images.irradiance_map, 2024,
                      PrefilterDistribution::lambertian);

    m_offline_images.prefiltered_cubemap.init({
        .format = ImageInfo::Format::RGBA32F,
        .width = m_offline_images.env_map.m_info.width,
        .height = m_offline_images.env_map.m_info.height,
        .num_levels = m_offline_images.env_map.m_info.num_levels,
        .num_faces = 6,
        .is_cubemap = true,
    });
    prefilter_cubemap(m_offline_images.env_map, m_offline_images.prefiltered_cubemap, 1024,
                      PrefilterDistribution::GGX);
}

void Renderer::generate_brdf_lut(Image &brdf_lut) {
    brdf_lut.init({.format = ImageInfo::Format::RGBA16F,
                   .width = 256,
                   .height = 256,
                   .num_levels = 1,
                   .num_faces = 1});

    glBindTextureUnit(0, brdf_lut.m_handle);
    glBindImageTexture(0, brdf_lut.m_handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    u32 compute_shader = load_shader("shaders/offline/brdf_lut.comp.glsl", GL_COMPUTE_SHADER);
    assert(compute_shader && "Failed to load brdf LUT generation compute shader");

    u32 pipeline;
    glGenProgramPipelines(1, &pipeline);
    glUseProgramStages(pipeline, GL_COMPUTE_SHADER_BIT, compute_shader);
    glBindProgramPipeline(pipeline);

    glDispatchCompute(brdf_lut.m_info.width / 16, brdf_lut.m_info.height / 16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void Renderer::generate_cubemap_from_equirectangular_new(const Image &eq_map,
                                                         const Sampler &eq_map_sampler,
                                                         ImageInfo::Format cubemap_format,
                                                         Image &result) {
    Pipeline pipeline;
    pipeline.init();

    std::array<VertexAttributeDescriptor, 1> attribs = {VertexAttributeDescriptor{
        .type = VertexAttributeDescriptor::Type::f32, .count = 3, .offset = 0}};
    pipeline.add_vertex_buffer(attribs, 3 * sizeof(f32),
                               std::span((u8 *)skybox_vertices, 36 * 3 * sizeof(f32)));

    pipeline.add_vertex_shader("shaders/SPIRV/cubemapgen.vert.spv");
    pipeline.add_fragment_shader("shaders/SPIRV/cubemapgen.frag.spv");
    pipeline.compile();

    u32 face_size = eq_map.m_info.width / 4;
    u32 mip_levels = (std::log2(face_size) + 1);

    result.init({
        .format = cubemap_format,
        .width = face_size,
        .height = face_size,
        .num_levels = mip_levels,
        .num_faces = 6,
        .is_cubemap = true,
    });

    u32 capture_fbo;
    glCreateFramebuffers(1, &capture_fbo);

    u32 capture_rbo;
    glCreateRenderbuffers(1, &capture_rbo);
    glNamedRenderbufferStorage(capture_rbo, GL_DEPTH_COMPONENT24, face_size, face_size);
    glNamedFramebufferRenderbuffer(capture_fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo);

    auto lambda = [this](u32 face_index, const glm::mat4 &curr_view) {
        (void)face_index;
        glNamedBufferSubData(m_ubo_matrices_handle, sizeof(glm::mat4), sizeof(glm::mat4),
                             glm::value_ptr(curr_view));
    };

    glm::mat4 capture_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glNamedBufferSubData(m_ubo_matrices_handle, 2 * sizeof(glm::mat4), sizeof(glm::mat4),
                         glm::value_ptr(capture_proj));

    glBindSampler(0, eq_map_sampler.m_handle);
    glBindTextureUnit(0, eq_map.m_handle);

    render_to_cubemap_faces(pipeline, face_size, result.m_handle, 0, lambda);
    glGenerateTextureMipmap(result.m_handle);

    pipeline.deinit();
}

void Renderer::prefilter_cubemap(const Image &env_map, const Image &result, u32 sample_count,
                                 PrefilterDistribution dist) {
    Pipeline pipeline;
    pipeline.init();

    std::array<VertexAttributeDescriptor, 1> attribs = {VertexAttributeDescriptor{
        .type = VertexAttributeDescriptor::Type::f32, .count = 3, .offset = 0}};
    pipeline.add_vertex_buffer(attribs, 3 * sizeof(f32),
                               std::span((u8 *)skybox_vertices, 36 * 3 * sizeof(f32)));

    pipeline.add_vertex_shader_src("shaders/offline/prefilter_env_map.vert.glsl");
    pipeline.add_fragment_shader_src("shaders/offline/prefilter_env_map.frag.glsl");
    pipeline.compile();
    pipeline.bind();

    glm::mat4 capture_proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glUniformMatrix4fv(5, 1, GL_FALSE, glm::value_ptr(capture_proj));
    glBindTextureUnit(0, env_map.m_handle);

    for (size_t mip = 0; mip < result.m_info.num_levels; ++mip) {
        // uniform uint sample_count
        glUniform1ui(0, sample_count);

        // uniform uint distribution;
        glUniform1ui(1, (u32)dist);

        // uniform float roughness;
        f32 roughness = (f32)mip / (f32)(result.m_info.num_levels - 1);
        glUniform1f(2, roughness);

        // uniform uint width;
        glUniform1ui(3, env_map.m_info.width);

        // uniform uint height;
        glUniform1ui(4, env_map.m_info.height);

        auto lambda = [this](u32 face_index, const glm::mat4 &curr_view) {
            (void)this;
            glUniformMatrix4fv(6, 1, GL_FALSE, glm::value_ptr(curr_view));
            (void)face_index;
        };

        render_to_cubemap_faces(pipeline, result.m_info.width >> mip, result.m_handle, mip, lambda);
    }

    pipeline.deinit();
}

}  // namespace engine
