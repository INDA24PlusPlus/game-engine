#ifndef _ASSET_IMPORTER_H
#define _ASSET_IMPORTER_H

#include <unordered_map>
#include <vector>
#include <string>
#include <tiny_gltf.h>

#include "../../../src/engine/Scene.h"

using namespace engine;

struct AssetImporter {
    u32 m_base_node;
    u32 m_base_mesh;

    // Maps the GLTF files node indices to our own.
    std::unordered_map<u32, u32> m_node_map;
    std::unordered_map<std::string, u32> m_mesh_names;

    std::vector<u8> m_indices;
    std::vector<Vertex> m_vertices;
    std::vector<Mesh> m_meshes;
    std::vector<Primitive> m_primitives;

    std::vector<Node> m_nodes;
    std::vector<u32> m_root_nodes;

    void load_asset(std::string path);
    void load_indices(const tinygltf::Model& model, const tinygltf::Accessor accessor);
    void load_vertices(const tinygltf::Model& model, const tinygltf::Primitive prim);
    void load_node(const tinygltf::Model& model, u32 gltf_node_index, u32 our_node_index);
    void load_primitive(const tinygltf::Model& model, const tinygltf::Primitive& prim);
    void load_meshes(const tinygltf::Model& model);
    void load_nodes(const tinygltf::Model& model);
};

#endif
