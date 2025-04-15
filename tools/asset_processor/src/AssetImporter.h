#ifndef _ASSET_IMPORTER_H
#define _ASSET_IMPORTER_H

#include <ktx.h>
#include <tiny_gltf.h>

#include <string>
#include <unordered_map>

#include "../../../src/engine/Scene.h"


using namespace engine;

struct AssetImporter {
    u32 m_base_node;
    u32 m_base_mesh;
    u32 m_base_texture;
    u32 m_base_image;
    u32 m_base_sampler;
    u32 m_base_material;

    AssetImporter() = delete;
    AssetImporter(std::string exe_path);

    std::string m_cache_dir;

    // Maps the GLTF files node indices to our own.
    std::unordered_map<u32, u32> m_node_map;

    // Mesh name -> mesh index
    std::unordered_map<std::string, u32> m_mesh_names;

    std::vector<u8> m_indices;
    std::vector<Vertex> m_vertices;
    std::vector<Mesh> m_meshes;
    std::vector<Primitive> m_primitives;

    std::vector<Node> m_nodes;
    std::vector<u32> m_root_nodes;

    std::vector<SamplerInfo> m_samplers;
    std::vector<ImageInfo> m_images;
    std::vector<TextureInfo> m_textures;
    std::vector<Material> m_materials;
    std::vector<u8> m_image_data;

    void load_asset(std::string path);
    void load_indices(const tinygltf::Model& model, const tinygltf::Accessor accessor);
    void load_vertices(const tinygltf::Model& model, const tinygltf::Primitive prim);
    void load_node(const tinygltf::Model& model, u32 gltf_node_index, u32 our_node_index);
    void load_primitive(const tinygltf::Model& model, const tinygltf::Primitive& prim);
    void load_meshes(const tinygltf::Model& model);
    void load_nodes(const tinygltf::Model& model);
    void load_textures(const tinygltf::Model& model);
    void load_images(const tinygltf::Model& model, const std::vector<bool>& is_srgb);
    void determine_required_images(const tinygltf::Model& model, std::vector<bool>& is_srgb);
    void load_samplers(const tinygltf::Model& model);
    void load_materials(const tinygltf::Model& model);
    void write_texture_to_image_data(ktxTexture2* texture);
    void compress_texture(ktxTexture2* texture);
    void load_image_data_into_texture(ktxTexture2* texture, const tinygltf::Image& image);
    ktxTexture2* write_to_texture_cache(const tinygltf::Image& image, bool is_srgb, u32 mip_levels,
                                        std::string_view path);
};

#endif
