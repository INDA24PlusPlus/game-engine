#include "assets.h"

#include "engine/arena.h"
#include "engine/platform.h"
#include "platform.h"

namespace engine::asset {


void AssetLoader::init(const char* path) {
    file = platform::open_file(path);
    if (file.handle == nullptr) {
        platform::fatal(std::format("Failed to open asset pack file at path {}", path));
    }

    auto total_file_size = platform::get_file_size(file);
    if (total_file_size == 0) {
        platform::fatal("Trying to read empty asset pack file?");
    }

    auto read_size = platform::read_from_file(file, { .ptr = (u8*)&header, .len = sizeof(Header) });
    if (read_size != sizeof(Header)) {
        platform::fatal("Failed to read entire asset pack header");
    }
}

void AssetLoader::deinit() {
    platform::close_file(file);
}

u64 AssetLoader::static_vertices_size() {
    return header.num_static_vertices * sizeof(Vertex);
}

u64 AssetLoader::skinned_vertices_size() {
    return header.num_skinned_vertices * sizeof(SkinnedVertex);
}

u64 AssetLoader::indices_size() {
    return header.num_indices * sizeof(IndexType);
}

u64 AssetLoader::gpu_data_size() {
    return static_vertices_size() + skinned_vertices_size() + indices_size();
}

u64 AssetLoader::offset_to_indics() {
    return static_vertices_size() + skinned_vertices_size();
}

template<typename T>
static T* read_from_ptr(u8* ptr, u64 count, u64& cursor) {
    auto ret = (T*)(ptr + cursor);
    cursor += count * sizeof(T);
    return ret;
}

AssetPack AssetLoader::load_cpu_data(Arena* arena) {
    auto mem = arena_alloc<u8>(arena, header.cpu_asset_size);
    assert(platform::read_from_file(file, mem) == header.cpu_asset_size && "Failed to read entire asset pack file");

    u64 cursor = 0;
    auto static_meshes = read_from_ptr<StaticMesh>(mem.ptr, header.num_static_meshes, cursor);
    auto skinned_meshes = read_from_ptr<SkinnedMesh>(mem.ptr, header.num_skinned_meshes, cursor);
    auto nodes = read_from_ptr<Node>(mem.ptr, header.num_nodes, cursor);
    auto bone_data = read_from_ptr<BoneData>(mem.ptr, header.num_bones, cursor);
    auto animations = read_from_ptr<Animation>(mem.ptr, header.num_animations, cursor);
    auto channels = read_from_ptr<NodeAnim>(mem.ptr, header.num_channels, cursor);
    auto translations = read_from_ptr<TranslationKey>(mem.ptr, header.num_translations, cursor);
    auto scalings = read_from_ptr<ScaleKey>(mem.ptr, header.num_scalings, cursor);
    auto rotations = read_from_ptr<RotationKey>(mem.ptr, header.num_rotations, cursor);
    auto global_inverse_matrices = read_from_ptr<glm::mat4>(mem.ptr, header.num_global_inverse_matrices, cursor);

    assert(cursor == mem.len && "We did not read all the asset data we loaded?");

    return {
        .static_meshes = { .ptr = static_meshes, .len = header.num_static_meshes },
        .skinned_meshes = { .ptr = skinned_meshes, .len = header.num_skinned_meshes },
        .nodes = { .ptr = nodes, .len = header.num_nodes },
        .bone_data = { .ptr = bone_data, .len = header.num_bones },
        .animations = { .ptr = animations, .len = header.num_animations },
        .animation_channels = { .ptr = channels, .len = header.num_channels },
        .translation_keys = { .ptr = translations, .len = header.num_translations },
        .scaling_keys = { .ptr = scalings, .len = header.num_scalings },
        .rotation_keys = { .ptr = rotations, .len = header.num_rotations },
        .global_inverse_matrices = { .ptr = global_inverse_matrices, .len = header.num_global_inverse_matrices },
    };
}

void AssetLoader::load_gpu_data(Slice<u8> staging_buffer) {
    auto size_read = platform::read_from_file(file, staging_buffer);
    if (size_read != header.gpu_asset_size) {
        platform::fatal("Failed to load all GPU asset pack data");
    }
}

}