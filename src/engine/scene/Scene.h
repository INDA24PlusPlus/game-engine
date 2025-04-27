#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


#include "AssetManifest.h"

#include "../core.h"
#include "../utils/logging.h"

namespace engine::loader {
struct AssetFileData;
}

namespace engine {
class Sampler;
class Image;

struct Vertex {
    glm::vec4 tangent;
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};


struct MeshTag;
using MeshHandle = TypedHandle<MeshTag>;
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

struct ImmutableNode {
    AssetManifest::Name name;
    glm::quat rotation;
    glm::vec3 translation;
    u32 child_index;
    glm::vec3 scale;
    u32 num_children;
    u32 mesh_index;
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


struct PrefabTag;
using PrefabHandle = TypedHandle<PrefabTag>;

struct Prefab {
    AssetManifest::Name name;
    u32 root_node_index;
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

class Scene {
public:
    void init(loader::AssetFileData& data);

    MeshHandle mesh_by_name(const std::string& name) const {
        if (m_manifest.m_name_to_mesh.find(name) == m_manifest.m_name_to_mesh.end()) {
            ERROR("{} is not a valid mesh name!", name);
            exit(1);
        }
        u32 index = m_manifest.m_name_to_mesh.at(name);
        return MeshHandle(index);
    }

    Prefab prefab_by_name(const std::string& name) const {
        if (m_manifest.m_name_to_prefab.find(name) == m_manifest.m_name_to_prefab.end()) {
            ERROR("{} is not a valid prefab name!", name);
            exit(1);
        }
        u32 index = m_manifest.m_name_to_prefab.at(name);
        return m_prefabs[index];
    }

    AssetManifest m_manifest;
    std::vector<Mesh> m_meshes;
    std::vector<Primitive> m_primitives;
    std::vector<Material> m_materials;
    std::vector<Sampler> m_samplers;
    std::vector<Image> m_images;
    std::vector<TextureInfo> m_textures;
    std::vector<Prefab> m_prefabs;
    std::vector<ImmutableNode> m_prefab_nodes;
};

}