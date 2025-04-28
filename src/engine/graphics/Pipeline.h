#pragma once

#include <cassert>
#include <span>
#include <string>

#include "../core.h"

namespace engine {

struct VertexAttributeDescriptor {
    enum class Type : u32 {
        f32,
    };

    Type type;
    u32 count;
    u32 offset;

    inline u32 size() const {
        u32 type_size;
        switch (type) {
            case Type::f32: {
                type_size = 4;
                break;
            }
            default: assert(0);
        }
        return type_size * count;
    }
};

// This class will not work once we switch to vulkan so don't get attached to it.
class Pipeline {
public:
    // Use this instead of constructor.
    void init();
    void deinit();

    void add_vertex_buffer(std::span<const VertexAttributeDescriptor> attributes, u32 stride, std::span<u8> vertex_data);
    void add_index_buffer(std::span<u8> indices);
    void add_vertex_shader(const std::string& path, std::span<u32> specialization_constants = {});
    void add_fragment_shader(const std::string& path, std::span<u32> specialization_constants = {});
    void compile();
    void bind();


    // Will disappear
    void add_vertex_shader_src(const std::string& path);
    void add_fragment_shader_src(const std::string& path);

    u32 m_program;
    u32 m_vao;

    u32 m_vbo;
    u32 m_ibo;
    u32 m_vshader;
    u32 m_fshader;
};

}