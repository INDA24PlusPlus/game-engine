#include "Pipeline.h"

#include <glad/glad.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <vector>

#include "../utils/logging.h"

namespace engine {

u32 static load_shader_binary(const char* path, u32 shader_type, std::span<u32> specialization_constants) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ERROR("Failed to open shader file at {}", path);
        return UINT32_MAX;
    }

    size_t file_size = file.tellg();
    std::vector<u32> shader_code(file_size / sizeof(u32));
    file.seekg(0);
    file.read((char*)shader_code.data(), file_size);
    file.close();

    assert(specialization_constants.size() < 256);
    std::array<u32, 256> spec_indices;
    for (u32 i = 0; i < spec_indices.size(); ++i) {
        spec_indices[i] = i;
    }

    u32 shader = glCreateShader(shader_type);
    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, shader_code.data(), shader_code.size() * sizeof(u32));
    glSpecializeShader(shader, "main", specialization_constants.size(), spec_indices.data(), specialization_constants.data());

    i32 compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        char msg[4096];
        glGetShaderInfoLog(shader, sizeof(msg), nullptr, msg);
        ERROR("Failed to compile shader {} with error: {}", path, msg);
        return UINT32_MAX;
    }

    return shader;
}

u32 static load_shader(const char* path, u32 shader_type) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ERROR("Failed to open shader file at {}", path);
        return UINT32_MAX;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();
    const char* source = src.c_str();
    GLint length = src.length();

    u32 shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    i32 compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        char msg[4096];
        glGetShaderInfoLog(shader, sizeof(msg), nullptr, msg);
        ERROR("Failed to compile shader {} with error: {}", path, msg);
        return UINT32_MAX;
    }

    return shader;
}

void Pipeline::init() {
    glCreateVertexArrays(1, &m_vao);
    m_program = glCreateProgram();
}

void Pipeline::deinit() {
    // Delete everything and let OpenGL decide what happens when we delete something that has not been created;
    glBindVertexArray(0);
    glUseProgram(0);
    glDeleteProgram(m_program);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ibo);
}

void Pipeline::add_vertex_shader(const std::string& path, std::span<u32> specialization_constants) {
    m_vshader = load_shader_binary(path.c_str(), GL_VERTEX_SHADER, specialization_constants);
}

void Pipeline::add_fragment_shader(const std::string& path, std::span<u32> specialization_constants) {
    m_fshader = load_shader_binary(path.c_str(), GL_FRAGMENT_SHADER, specialization_constants);
}

void Pipeline::add_vertex_shader_src(const std::string& path) {
    m_vshader = load_shader(path.c_str(), GL_VERTEX_SHADER);
}

void Pipeline::add_fragment_shader_src(const std::string& path) {
    m_fshader = load_shader(path.c_str(), GL_FRAGMENT_SHADER);
}

void Pipeline::compile() {
    glAttachShader(m_program, m_vshader);
    glAttachShader(m_program, m_fshader);
    glLinkProgram(m_program);

    i32 linked;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);

    if (!linked) {
        char msg[4096];
        glGetProgramInfoLog(m_program, sizeof(msg), nullptr, msg);
        ERROR("Failed to link shader program with error: {}", msg);
        exit(1);
    }
    glDeleteShader(m_fshader);
    glDeleteShader(m_vshader);
}

void Pipeline::bind() {
    glUseProgram(m_program);
    glBindVertexArray(m_vao);
}


void Pipeline::add_vertex_buffer_from_buffer(std::span<const VertexAttributeDescriptor> attributes, u32 stride, u32 buffer) {
    m_vbo = buffer;
    glVertexArrayVertexBuffer(m_vao, 0, buffer, 0, stride);
    for (size_t i = 0; i < attributes.size(); ++i) {
        const auto& attrib = attributes[i];
        u32 gl_type;
        switch (attrib.type) {
            case VertexAttributeDescriptor::Type::f32: {
                gl_type = GL_FLOAT;
                break;
            }
            default: assert(0);
        }

        glVertexArrayAttribFormat(m_vao, i, attrib.count, gl_type, GL_FALSE, attrib.offset);
        glVertexArrayAttribBinding(m_vao, i, 0);
        glEnableVertexArrayAttrib(m_vao, i);
    }
}

void Pipeline::add_vertex_buffer(std::span<const VertexAttributeDescriptor> attributes, u32 stride, std::span<u8> vertex_data) {
    u32 buffer;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, vertex_data.size(), vertex_data.data(), 0);

    add_vertex_buffer_from_buffer(attributes, stride, buffer);
}


void Pipeline::add_index_buffer(std::span<u8> indices) {
    glCreateBuffers(1, &m_ibo);
    glNamedBufferStorage(m_ibo, indices.size(), indices.data(), 0);
    glVertexArrayElementBuffer(m_vao, m_ibo);
}


}
