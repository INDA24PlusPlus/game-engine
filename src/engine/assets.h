#ifndef _ASSETS_H
#define _ASSETS_H

#include "core.h"
#include "arena.h"
#include "platform.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

namespace engine::asset {

struct Header {
    u32 cpu_asset_size;
    u32 gpu_asset_size;
    u32 num_static_meshes;
    u32 num_skinned_meshes;
    u32 num_bones;
    u32 num_nodes;
    u32 num_animations;
    u32 num_channels; // Must be same as num bones.
    u32 num_translations;
    u32 num_scalings;
    u32 num_rotations;
    u32 num_global_inverse_matrices;


    u32 num_static_vertices;
    u32 num_skinned_vertices;
    u32 num_indices;
};

struct StaticMesh {
    u32 base_vertex;
    u32 num_vertices;
    u32 base_index;
    u32 num_indices;
};

struct SkinnedMesh {
    u32 base_vertex;
    u32 num_vertices;
    u32 base_index;
    u32 num_indices;
    u32 base_bone;
    u32 num_bones;
};

struct Vertex {
    f32 pos[3];
    f32 normal[3];
    f32 uv[2];
};

constexpr u32 bones_per_vertex = 4;

struct SkinnedVertex {
    f32 pos[3];
    u8 bone_indices[bones_per_vertex];
    u8 bone_weights[bones_per_vertex];
};

struct BoneData {
    glm::mat4 offset;
    glm::mat4 final;
};

struct Animation {
    f32 duration;
    f32 ticks_per_second;
    u32 channel_index;
    u32 channel_count;
};


struct TranslationKey {
    glm::vec3 translation;
    f32 time;
};

struct ScaleKey {
    glm::vec3 scale;
    f32 time;
};

struct RotationKey {
    glm::quat rotation;
    f32 time;
};

struct NodeAnim {
    u32 is_identity;
    u32 translation_index;
    u32 num_translations;

    u32 scaling_index;
    u32 num_scalings;

    u32 rotation_index;
    u32 num_rotations;
};

struct Node {
    enum class Kind {
        none,
        bone,
        mesh,
    };

    struct MeshPayload {
        u32 num_meshes;
        u32 mesh_indices[4];
    };

    glm::mat4 transform;
    u32 child_count;
    u32 child_index;
    Kind kind;
    union {
        MeshPayload mesh_data;
        u32 bone_index;
    };
};

typedef u16 IndexType;

struct NodeHandle {
    void* handle;
};

struct AssetPack {
    Slice<StaticMesh> static_meshes;
    Slice<SkinnedMesh> skinned_meshes;
    Slice<Node> nodes;
    Slice<BoneData> bone_data;
    Slice<Animation> animations;
    Slice<NodeAnim> animation_channels;
    Slice<TranslationKey> translation_keys;
    Slice<ScaleKey> scaling_keys;
    Slice<RotationKey> rotation_keys;
    Slice<glm::mat4> global_inverse_matrices;
};

struct AssetLoader {
    struct GPUPointers {
        Vertex* static_vertices;
        SkinnedVertex* skinned_vertices;
        IndexType* indices;
    };

    platform::OSHandle file;
    Header header;

    void init(const char* path);
    void deinit();

    u64 static_vertices_size();
    u64 skinned_vertices_size();
    u64 indices_size();
    u64 gpu_data_size();

    u64 offset_to_indics();

    AssetPack load_cpu_data(Arena* arena);
    void load_gpu_data(Slice<u8> staging_buffer);
};

}

#endif
