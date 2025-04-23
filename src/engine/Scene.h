#ifndef _SCENE_H
#define _SCENE_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "core.h"

namespace engine {

struct MeshTag;
using MeshHandle = TypedHandle<MeshTag>;

struct NodeTag;
using NodeHandle = TypedHandle<NodeTag>;

struct Vertex {
    glm::vec4 tangent;
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Mesh {
    u32 primitive_index;
    u32 num_primitives;
    u32 node_index;
};

struct Primitive {
    u32 base_vertex;
    u32 num_vertices;
    u32 indices_start;
    u32 indices_end;
    u32 index_type;
    u32 material_index;

    inline u32 num_indices() const {
        u32 len = indices_end - indices_start;
        return len / (index_type - 5121);
    }
};

struct Node {
    glm::quat rotation;
    glm::vec3 translation;
    u32 child_index;
    glm::vec3 scale;
    u32 num_children;
};

struct AssetHeader {
    u32 version;
    u32 num_indices;
    u32 num_vertices;
    u32 num_meshes;
    u32 num_primitives;
    u32 num_nodes;
    u32 num_root_nodes;
    u32 num_name_bytes;
    u32 num_samplers;
    u32 num_images;
    u32 num_textures;
    u32 num_materials;
    u64 num_image_bytes;
};

// These have "Info" in the name cause they don't contain any actual GPU state.
struct SamplerInfo {
    u32 min_filter;
    u32 mag_filter;
    u32 wrap_t;
    u32 wrap_s;
};

struct TextureInfo {
    u32 sampler_index;
    u32 image_index;
};

struct ImageInfo {
    enum class Format : u32 {
        BC7_RGBA_SRGB,
        BC7_RGBA,
        BC5_RG,
        RGB32F,
        RGBA32F,
        RGB16F,
        RGBA16F,
        RGB8_UNORM,
    };

    Format format;
    u32 width;
    u32 height;
    u32 num_levels;
    u32 num_faces;
    b32 is_compressed;
    b32 is_cubemap;
    u64 image_data_index;

    inline u32 level_size_bc7_and_bc5(u32 level) const {
        u32 block_width = ((width >> level) + 3) / 4;
        u32 block_height = ((height >> level) + 3) / 4;
        u32 size_in_bytes = block_width * block_height * 16;
        return size_in_bytes;
    }

    inline u32 level_size_uncompressed(u32 level, u32 bytes_per_component,
                                       u32 num_components) const {
        u32 level_width = width >> level;
        u32 level_height = height >> level;
        return level_width * level_height * num_components * bytes_per_component;
    }

    inline u32 level_size(u32 level) const {
        switch (format) {
            case Format::BC5_RG:
            case Format::BC7_RGBA:
            case Format::BC7_RGBA_SRGB:
                return level_size_bc7_and_bc5(level);
            case Format::RGB32F:
                return level_size_uncompressed(level, 4, 3);
            case Format::RGBA32F:
                return level_size_uncompressed(level, 4, 4);
            case Format::RGBA16F:
                return level_size_uncompressed(level, 2, 4);
            case Format::RGB16F:
                return level_size_uncompressed(level, 2, 3);
            case Format::RGB8_UNORM:
                return level_size_uncompressed(level, 1, 3);
            default:
                assert(0);
        }
    }

    inline u32 level_offset(u32 face, u32 level) const {
        assert(level < num_levels);
        assert(face < num_faces);

        u64 offset = 0;
        for (u32 cur_level = 0; cur_level < level; ++cur_level) {
            u32 size_in_bytes = level_size(cur_level);
            offset += size_in_bytes;
        }
        offset *= (face + 1);

        return offset;
    }
};

struct Material {
    enum class Flags : u32 {
        has_base_color_texture = 1 << 0,
        has_metallic_roughness_texture = 1 << 1,
        has_normal_map = 1 << 2,
        has_occlusion_map = 1 << 3,
        has_emission_map = 1 << 4,
    };

    Flags flags;
    glm::vec4 base_color_factor;
    f32 metallic_factor;
    f32 roughness_factor;
    u32 base_color_texture;
    u32 metallic_roughness_texture;
    u32 normal_map;
    f32 normal_map_scale;
    u32 occlusion_map;
    f32 occlusion_strength;
    u32 emission_map;
    glm::vec3 emission_factor;
};

class Scene {
   public:
    // Loaded from disk
    std::span<u8> m_indices;
    std::span<Vertex> m_vertices;
    std::span<Mesh> m_meshes;
    std::span<Primitive> m_primitives;

    std::span<Node> m_nodes;
    std::span<u32> m_root_nodes;

    std::span<SamplerInfo> m_samplers;
    std::span<ImageInfo> m_images;
    std::span<TextureInfo> m_textures;
    std::span<Material> m_materials;
    std::span<u8> m_image_data;

    // Compute
    std::vector<glm::mat4> m_global_node_transforms;

    void load_asset_file(const char* path);
    void compute_global_node_transforms();
    MeshHandle mesh_from_name(std::string name);

   private:
    void read_mesh_names(u8*& ptr);
    void calc_global_node_transform(NodeHandle node_handle, const glm::mat4& parent_transform);
    std::vector<u8> m_asset_file_mem;
    std::unordered_map<std::string, MeshHandle> m_names_to_mesh;
};

}  // namespace engine

#endif
