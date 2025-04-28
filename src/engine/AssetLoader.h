#pragma once

#include <span>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core.h"

#include "scene/AssetManifest.h"
#include "scene/Scene.h"
#include "graphics/Sampler.h"


// The typers defined here are special types that are practically only used
// in the asset file and will be marshalled to a format more appropriate for the engine.
// For example the node struct defined here assume a hierarchy that is immutable.
namespace engine::loader {

// clang-format off
// "Documentation" for the asset file:
//struct AssetFile {
//  AssetHeader header;
//  u8 indice[num_indices]; (consist of either u16 or u32s as indices)
//  Vertex vertices[num_vertices];
//  Mesh meshes[num_meshes];
//  Primitive primitives[num_primitives];
//  ImmutableNode prefab_nodes[num_prefab_nodes];
//  SamplerInfo samplers[num_samplers];
//  ImageInfo images[num_images];
//  TextureInfo textures[num_textures];
//  Material materials[num_materials];
//  u8 name_bytes[num_name_bytes]; // The bytes making up the names of the various assets. First byte of each "name" is the length of the name.
//  AssetManifest::Name mesh_names[num_meshes];
//  AssetManifest::Name prefab_names[num_prefabs];
//  u8 image_data[num_image_bytes]; // Raw image bytes. Format determine by image info.
//}
// clang-format on
struct AssetHeader {
    u32 version;
    u32 num_indices;
    u32 num_vertices;
    u32 num_meshes;
    u32 num_primitives;
    u32 num_prefab_nodes;
    u32 num_samplers;
    u32 num_images;
    u32 num_textures;
    u32 num_materials;
    u32 num_prefabs;

    // The manifest description begins here.
    u32 num_name_bytes;

    // Image bytes are always last so that we can free it once it is on the GPU.
    u64 num_image_bytes;
};

struct SamplerInfo {
    Sampler::Filter mag_filter;
    Sampler::Filter min_filter;
    Sampler::MipmapMode mipmap_mode;
    Sampler::AddressMode address_mode_u;
    Sampler::AddressMode address_mode_v;
    Sampler::AddressMode address_mode_w;
};

struct Prefab {
    u32 name_index;
    u32 root_node_index;
};

struct AssetFileData {
    std::vector<u8> backing_memory;
    std::span<u8> indices;
    std::span<Vertex> vertices;
    std::span<Mesh> meshes;
    std::span<Primitive> primitives;
    std::span<ImmutableNode> prefab_nodes;
    std::span<u32> root_prefab_nodes;
    std::span<SamplerInfo> samplers;
    std::span<ImageInfo> images;
    std::span<TextureInfo> textures;
    std::span<Material> materials;
    std::span<u8> image_data;

    // Needed to construct the manifest
    std::span<u8> name_bytes;
    std::span<AssetManifest::Name> mesh_names;
    std::span<AssetManifest::Name> prefab_names;
};

AssetFileData load_asset_file(const char* path);

}  // namespace engine