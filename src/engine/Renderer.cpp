#include "Renderer.h"

#include <cassert>
#include <cstdint>
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

#include "utils/logging.h"

namespace engine {

Renderer::Renderer(LoadProc load_proc) : m_scene_loaded{false}, m_pass_in_progress{false} {
    if (!gladLoadGLLoader((GLADloadproc)load_proc)) {
        ERROR("Failed to load OpenGL function pointers");
        exit(1);
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void Renderer::make_resources_for_scene(const Scene& scene) {
    assert(!m_scene_loaded);
    glCreateVertexArrays(1, &m_vao);
    glVertexArrayElementBuffer(m_vao, m_ibo);

    glCreateBuffers(1, &m_ibo);
    glNamedBufferStorage(m_ibo, sizeof(u32) * scene.m_indices.size(), scene.m_indices.data(), 0);
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

    m_scene_loaded = true;
}

void Renderer::clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::begin_pass(const Camera& camera, u32 width, u32 height) {
    assert(!m_pass_in_progress);
    glBindVertexArray(m_vao);
    glBindProgramPipeline(m_pipeline);

    f32 aspect_ratio = (f32)width / (f32)height;
    auto projection = glm::perspective(glm::radians(90.0f), aspect_ratio, 0.1f, 1000.0f);
    auto view = camera.get_view_matrix();

    glProgramUniformMatrix4fv(m_vshader, 1, 1, GL_FALSE, glm::value_ptr(view));
    glProgramUniformMatrix4fv(m_vshader, 2, 1, GL_FALSE, glm::value_ptr(projection));
    m_pass_in_progress = true;
}

void Renderer::end_pass() {
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
        glDrawElementsBaseVertex(GL_TRIANGLES, prim.num_indices, GL_UNSIGNED_INT,
                                 (void*)(sizeof(u32) * prim.base_index), prim.base_vertex);
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
