#include "Renderer.h"

#include <glad/glad.h>

#include <cassert>
#include <cstdint>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <string>

#include "utils/logging.h"

namespace engine {

static void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
                                   GLsizei length, const char* message, const void* userParam) {
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
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &m_max_texture_filtering);

    INFO("Initialized renderer");
}

f32 Renderer::get_max_texture_filtering_level() const {
    return m_max_texture_filtering;
}

void Renderer::set_texture_filtering_level(f32 level) {
    for (size_t i = 0; i < m_sampler_handles.size(); ++i) {
        glSamplerParameterf(m_sampler_handles[i], GL_TEXTURE_MAX_ANISOTROPY, level);
    }
}

void Renderer::make_resources_for_scene(const Scene& scene) {
    INFO("Creating renderer state for scene data.");
    assert(!m_scene_loaded);
    glCreateVertexArrays(1, &m_vao);

    glCreateBuffers(1, &m_ibo);
    glNamedBufferStorage(m_ibo, scene.m_indices.size(), scene.m_indices.data(), 0);
    glVertexArrayElementBuffer(m_vao, m_ibo);

    glCreateBuffers(1, &m_vbo);
    glNamedBufferStorage(m_vbo, sizeof(Vertex) * scene.m_vertices.size(), scene.m_vertices.data(),
                         0);

    u32 vertex_buffer_index = 0;
    glVertexArrayVertexBuffer(m_vao, vertex_buffer_index, m_vbo, 0, sizeof(Vertex));

    u32 a_pos = 0;
    glVertexArrayAttribFormat(m_vao, a_pos, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));
    glVertexArrayAttribBinding(m_vao, a_pos, vertex_buffer_index);
    glEnableVertexArrayAttrib(m_vao, a_pos);

    u32 a_normal = 1;
    glVertexArrayAttribFormat(m_vao, a_normal, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribBinding(m_vao, a_normal, vertex_buffer_index);
    glEnableVertexArrayAttrib(m_vao, a_normal);

    u32 a_uv = 2;
    glVertexArrayAttribFormat(m_vao, a_uv, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
    glVertexArrayAttribBinding(m_vao, a_uv, vertex_buffer_index);
    glEnableVertexArrayAttrib(m_vao, a_uv);

    m_vshader = load_shader("shaders/basic.vert.glsl", GL_VERTEX_SHADER);
    assert(m_vshader && "Failed to load initial vertex shader");
    m_fshader = load_shader("shaders/basic.frag.glsl", GL_FRAGMENT_SHADER);
    assert(m_fshader && "Failed to load initial fragment shader");

    glGenProgramPipelines(1, &m_pipeline);
    glUseProgramStages(m_pipeline, GL_VERTEX_SHADER_BIT, m_vshader);
    glUseProgramStages(m_pipeline, GL_FRAGMENT_SHADER_BIT, m_fshader);

    m_texture_handles.resize(scene.m_images.size());
    glCreateTextures(GL_TEXTURE_2D, m_texture_handles.size(), m_texture_handles.data());
    for (size_t i = 0; i < scene.m_images.size(); ++i) {
        const auto& image = scene.m_images[i];
        u32 format =
            image.is_srb ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : GL_COMPRESSED_RGBA_BPTC_UNORM;

        glTextureStorage2D(m_texture_handles[i], image.num_levels, format, image.width,
                           image.height);

        for (u32 level = 0; level < image.num_levels; ++level) {
            u32 level_size = image.level_size(level);
            u32 level_offset = image.level_offset(level);
            u32 level_width = image.width >> level;
            u32 level_height = image.height >> level;
            glCompressedTextureSubImage2D(m_texture_handles[i], level, 0, 0, level_width,
                                          level_height, format, level_size,
                                          &scene.m_image_data[level_offset]);
        }
    }

    m_sampler_handles.resize(scene.m_samplers.size());
    glCreateSamplers(m_sampler_handles.size(), m_sampler_handles.data());
    for (size_t i = 0; i < m_sampler_handles.size(); ++i) {
        const auto& sampler = scene.m_samplers[i];
        glSamplerParameteri(m_sampler_handles[i], GL_TEXTURE_MAG_FILTER, sampler.mag_filter);
        glSamplerParameteri(m_sampler_handles[i], GL_TEXTURE_MIN_FILTER, sampler.min_filter);
        glSamplerParameteri(m_sampler_handles[i], GL_TEXTURE_WRAP_S, sampler.wrap_s);
        glSamplerParameteri(m_sampler_handles[i], GL_TEXTURE_WRAP_T, sampler.wrap_t);
    }

    m_scene_loaded = true;
    INFO("Created renderer state for scene object.");
}

void Renderer::clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

void Renderer::begin_pass(const Camera& camera, u32 width, u32 height) {
    assert(!m_pass_in_progress);
    glBindVertexArray(m_vao);
    glBindProgramPipeline(m_pipeline);
    glEnable(GL_FRAMEBUFFER_SRGB);

    f32 aspect_ratio = (f32)width / (f32)height;
    auto projection = glm::perspective(glm::radians(90.0f), aspect_ratio, 0.1f, 1000.0f);
    auto view = camera.get_view_matrix();

    glProgramUniformMatrix4fv(m_vshader, 1, 1, GL_FALSE, glm::value_ptr(view));
    glProgramUniformMatrix4fv(m_vshader, 2, 1, GL_FALSE, glm::value_ptr(projection));
    m_pass_in_progress = true;
}

void Renderer::end_pass() {
    glDisable(GL_FRAMEBUFFER_SRGB);
    assert(m_pass_in_progress);
    m_pass_in_progress = false;
}

void Renderer::draw_mesh(const Scene& scene, MeshHandle mesh_handle, const glm::mat4& transform) {
    // FIXME: Do the actual draw calls in end_pass
    const auto& mesh = scene.m_meshes[mesh_handle.get_value()];
    const auto& node_transform = transform * scene.m_global_node_transforms[mesh.node_index];
    glProgramUniformMatrix4fv(m_vshader, 0, 1, GL_FALSE, glm::value_ptr(node_transform));

    for (size_t i = 0; i < mesh.num_primitives; ++i) {
        const auto& prim = scene.m_primitives[mesh.primitive_index + i];
        const auto& material = scene.m_materials[prim.material_index];
        const auto& base_color_texture = scene.m_textures[material.base_color_texture];

        glBindSampler(0, m_sampler_handles[base_color_texture.sampler_index]);
        glBindTextureUnit(0, m_texture_handles[base_color_texture.image_index]);

        auto num_indices = prim.num_indices();
        u64 byte_offset = prim.indices_start;

        glDrawElementsBaseVertex(GL_TRIANGLES, num_indices, prim.index_type, (void*)byte_offset,
                                 prim.base_vertex);
    }
}

u32 Renderer::load_shader(const char* path, u32 shader_type) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ERROR("Failed to open shader file at {}", path);
        return UINT32_MAX;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();
    const char* source = src.c_str();
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

}  // namespace engine
