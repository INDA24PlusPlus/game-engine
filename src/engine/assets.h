#ifndef _ASSETS_H
#define _ASSETS_H

#include "core.h"
#include <glm/glm.hpp>

namespace engine::asset {

struct Header {
    u32 num_static_meshes;
    u32 num_skinned_meshes;
    u32 num_static_vertices;
    u32 num_skinned_vertices;
    u32 num_indices;
    u32 num_bones;
    u32 num_nodes;
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

static u64 total_size_of_assets(const Header& header) {
    return sizeof(IndexType) * header.num_indices
         + sizeof(Vertex) * header.num_static_vertices
         + sizeof(SkinnedVertex) * header.num_skinned_vertices;
}

static u64 get_static_mesh_offset(const Header& header) {
    (void)header;
    return sizeof(Header);
}

static u64 get_skinned_mesh_offset(const Header& header) {
    return get_static_mesh_offset(header) + header.num_static_meshes * sizeof(StaticMesh);
}

static u64 get_node_offset(const Header& header) {
    return get_skinned_mesh_offset(header) + header.num_skinned_meshes * sizeof(SkinnedMesh);
}

static u64 metadata_size(const Header& header) {
    return get_node_offset(header) + header.num_nodes * sizeof(Node);
}

static u64 get_static_vertex_asset_offset(const Header& header) {
    (void)header;
    return 0;
}

static u64 get_skinned_vertex_asset_offset(const Header& header) {
    return get_static_vertex_asset_offset(header) + sizeof(Vertex) * header.num_static_vertices;
}

static u64 get_index_asset_offset(const Header& header) {
    return get_skinned_vertex_asset_offset(header) + sizeof(SkinnedVertex) * header.num_skinned_vertices;
}

}
#endif
