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
    u32 width;
    u32 height;
    u32 num_levels;
    u32 is_srb;
    u64 image_data_index;

    inline u32 level_size(u32 desired_level) const {
        u32 block_width = ((width >> desired_level) + 3) / 4;
        u32 block_height = ((height >> desired_level) + 3) /4;
        u32 size_in_bytes = block_width * block_height * 16;
        return size_in_bytes;
    }

    inline u64 level_offset(u32 desired_level) const {
        assert(desired_level < num_levels);
        u64 offset = image_data_index;
        for (u32 level = 0; level < desired_level; ++level) {
            u32 size_in_bytes = level_size(level);
            offset += size_in_bytes;
        }

        return offset;
    }
};

struct Material {
    enum class Flags : u32 {
        has_base_color_texture = 1 << 0,
    };

    Flags flags;
    u32 base_color_texture;
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


}

#endif
