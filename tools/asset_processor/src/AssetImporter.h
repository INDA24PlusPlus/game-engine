
#ifndef _ASSET_IMPORTER_H
#define _ASSET_IMPORTER_H

#include <span>
#include <unordered_map>
#include <vector>
#include <string>
#include <tiny_gltf.h>

#include "../../../src/engine/Scene.h"

using namespace engine;

struct AssetImporter {
    // Maps the GLTF files node indices to our own.
    std::unordered_map<u32, u32> m_node_map;

    std::vector<u32> m_indices;
    std::vector<Vertex> m_vertices;
    std::vector<Mesh> m_meshes;
    std::vector<Primitive> m_primitives;

    std::vector<Node> m_nodes;
    std::vector<u32> m_root_nodes;

    std::vector<u32> m_mesh_name_indices;
    std::vector<char> m_name_data;

    // Resets every time a new asset file is loaded.
    u32 m_base_root_node;
    u32 m_base_mesh;

    void load_asset(std::string path);
    void load_indices(const tinygltf::Model& model, const tinygltf::Accessor accessor);
    void load_vertices(const tinygltf::Model& model, const tinygltf::Primitive prim);
    void load_node(u32 base_node, u32 node_index, u32 gltf_node_index, std::span<const tinygltf::Node> nodes);
    void load_primitive(const tinygltf::Model& model, const tinygltf::Primitive& prim);
    void load_meshes(const tinygltf::Model& model);
    u32 load_nodes(const tinygltf::Model& model);
};

struct AccessorIterator {
    const u8* data;
    int stride;
    int index;
    int count;

    void init(const tinygltf::Model& model, const tinygltf::Accessor& accessor);

    template<typename T>
    bool next(T& value) {
        if (index >= count) return false;

        value = *(T*)(data + index * stride);
        index += 1;

        return true;
    }
};

#endif
