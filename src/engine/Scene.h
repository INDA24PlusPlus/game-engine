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
    u32 base_index;
    u32 num_indices;
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
};

class Scene {
public:
    // Loaded from disk
    std::span<u32> m_indices;
    std::span<Vertex> m_vertices;
    std::span<Mesh> m_meshes;
    std::span<Primitive> m_primitives;

    std::span<Node> m_nodes;
    std::span<u32> m_root_nodes;

    // Computed
    std::vector<glm::mat4> m_global_node_transforms;

    void load_asset_file(const char* path);
    void compute_global_node_transforms();
    MeshHandle mesh_from_name(std::string name);
private:
    void calc_global_node_transform(NodeHandle node_handle, const glm::mat4& parent_transform);
    std::vector<u8> m_asset_file_mem;
    std::unordered_map<std::string, MeshHandle> m_names_to_mesh;
};


}

#endif