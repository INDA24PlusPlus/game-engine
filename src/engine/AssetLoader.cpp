#include "AssetLoader.h"

#include <fstream>

#include "engine/scene/AssetManifest.h"
#include "scene/Scene.h"
#include "utils/logging.h"

namespace engine::loader {

template <typename T>
static std::span<T> read_asset_data(u8*& ptr, u32 count, u8* end_of_file_Ptr) {
    size_t size_to_read = count * sizeof(T);
    if (ptr + size_to_read > end_of_file_Ptr) {
        ERROR(
            "Tried to read past the end of the asset file! This is a bug or the the asset format "
            "was updated and changes were not made to the parser! (Also remember to update the "
            "version number)");
        exit(1);
    }

    auto ret = std::span<T>((T*)ptr, count);
    ptr += size_to_read;
    return ret;
}

AssetFileData load_asset_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ERROR("Failed to open asset file at {}", path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    AssetFileData asset_file;
    asset_file.backing_memory = std::vector<u8>(size);
    if (!file.read((char*)asset_file.backing_memory.data(), size)) {
        ERROR("Failed to read all the contents of the asset file: {}", path);
    }

    file.close();

    constexpr u32 expected_version = 2;

    AssetHeader* header = (AssetHeader*)asset_file.backing_memory.data();
    if (header->version != expected_version) {
        ERROR(
            "Expected asset file version of {} but got {}, re-build and re-run the asset "
            "processor!",
            expected_version, header->version);
        exit(1);
    }

    INFO("------- Asset Header -------");
    INFO("Header version: {}", header->version);
    INFO("Num Indices: {}", header->num_indices);
    INFO("Num Vertices: {}", header->num_vertices);
    INFO("Num Meshe: {}", header->num_meshes);
    INFO("Num Primitives: {}", header->num_primitives);
    INFO("Num samplers: {}", header->num_samplers);
    INFO("Num images: {}", header->num_images);
    INFO("Num textures: {}", header->num_textures);
    INFO("Num materials: {}", header->num_materials);
    INFO("Num prefabs: {}", header->num_prefabs);
    INFO("Num name bytes: {}", header->num_name_bytes);
    INFO("Num image bytes: {}", header->num_image_bytes);
    INFO("Asset file is {} bytes ({} MB)", asset_file.backing_memory.size(),
         asset_file.backing_memory.size() >> 20);

    u8* end_ptr = asset_file.backing_memory.data() + asset_file.backing_memory.size();
    u8* ptr = asset_file.backing_memory.data() + sizeof(AssetHeader);

    asset_file.indices = read_asset_data<u8>(ptr, header->num_indices, end_ptr);
    asset_file.vertices = read_asset_data<Vertex>(ptr, header->num_vertices, end_ptr);
    asset_file.meshes = read_asset_data<Mesh>(ptr, header->num_meshes, end_ptr);
    asset_file.primitives = read_asset_data<Primitive>(ptr, header->num_primitives, end_ptr);
    asset_file.prefab_nodes = read_asset_data<ImmutableNode>(ptr, header->num_prefab_nodes, end_ptr);
    asset_file.root_prefab_nodes = read_asset_data<u32>(ptr, header->num_prefabs, end_ptr);
    asset_file.samplers = read_asset_data<SamplerInfo>(ptr, header->num_samplers, end_ptr);
    asset_file.images = read_asset_data<ImageInfo>(ptr, header->num_images, end_ptr);
    asset_file.textures = read_asset_data<TextureInfo>(ptr, header->num_textures, end_ptr);
    asset_file.materials = read_asset_data<Material>(ptr, header->num_materials, end_ptr);
    asset_file.name_bytes = read_asset_data<u8>(ptr, header->num_name_bytes, end_ptr);
    asset_file.mesh_names = read_asset_data<AssetManifest::Name>(ptr, header->num_meshes, end_ptr);
    asset_file.prefab_names = read_asset_data<AssetManifest::Name>(ptr, header->num_prefabs, end_ptr);
    asset_file.image_data = read_asset_data<u8>(ptr, header->num_image_bytes, end_ptr);

    size_t bytes_read = (size_t)(ptr - asset_file.backing_memory.data());
    if (bytes_read != asset_file.backing_memory.size()) {
        ERROR("Asset file is {} bytes but we only read {}, Diff is {}. This should never happen.",
              asset_file.backing_memory.size(), bytes_read,
              (int)asset_file.backing_memory.size() - (int)bytes_read);
        exit(1);
    }

    return asset_file;
}

};  // namespace engine::loader