#include "AssetImporter.h"

#include <encoder/basisu_resampler.h>
#include <ktx.h>
#include <stb_image.h>
#include <tiny_gltf.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <print>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "../../../src/engine/utils/logging.h"

constexpr bool verbose_accessor_logging = false;

inline static void update_fnv1a_hash(u64 *hash, u8 byte) {
    const u64 fnv_prime = 1099511628211ull;
    *hash ^= byte;
    *hash *= fnv_prime;
}

static u64 hash_fnv1a(std::span<const u8> data) {
    const u64 fnv_offset = 14695981039346656037ull;

    u64 hash = fnv_offset;
    for (uint8_t byte : data) {
        update_fnv1a_hash(&hash, byte);
    }

    return hash;
}

static f32 srgb_to_linear(f32 srgb) {
    return srgb <= 0.04045f ? srgb * (1.0f / 12.92f)
                            : std::pow((srgb + 0.055f) * (1.0f / 1.055f), 2.4f);
}

static f32 linear_to_srgb(f32 linear) {
    return linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

static void check_resampler_status(basisu::Resampler &resampler, const char *filter) {
    using Status = basisu::Resampler::Status;
    switch (resampler.status()) {
        case Status::STATUS_OKAY:
            break;
        case Status::STATUS_OUT_OF_MEMORY:
            ERROR("Resampler or Resampler::put_line out of memory.");
            exit(1);
        case Status::STATUS_BAD_FILTER_NAME:
            ERROR("Unknown filter: {}", filter);
            exit(1);
        case Status::STATUS_SCAN_BUFFER_FULL:
            ERROR("Resampler::put_line scan buffer full.");
            exit(1);
    }
}

// Based on the resample function from the KTX 2.0 toktx tool.
template <typename CompType, u32 num_comps>
static void resample(u32 dst_width, u32 dst_height, std::span<CompType> dst_pixels, u32 src_width,
                     u32 src_height, const std::vector<CompType> &src_pixels, bool is_srgb) {
    if (std::max(src_width, src_height) > BASISU_RESAMPLER_MAX_DIMENSION ||
        std::max(dst_width, dst_height) > BASISU_RESAMPLER_MAX_DIMENSION) {
        ERROR("Image larger than max supported resampler dimension of {}",
              BASISU_RESAMPLER_MAX_DIMENSION);
        exit(1);
    }

    auto filter = "lanczos4";
    f32 filter_scale = 1.0f;
    auto wrap_mode = basisu::Resampler::Boundary_Op::BOUNDARY_CLAMP;

    const bool is_hdr = std::is_floating_point_v<CompType>;

    std::array<std::vector<f32>, num_comps> samples;
    std::array<std::unique_ptr<basisu::Resampler>, num_comps> resamplers;

    for (size_t i = 0; i < num_comps; ++i) {
        resamplers[i] = std::make_unique<basisu::Resampler>(
            src_width, src_height, dst_width, dst_height, wrap_mode, 0.0f, is_hdr ? 0.0f : 1.0f,
            filter, i == 0 ? nullptr : resamplers[0]->get_clist_x(),
            i == 0 ? nullptr : resamplers[0]->get_clist_y(), filter_scale, filter_scale, 0.0f,
            0.0f);
        check_resampler_status(*resamplers[i], filter);
        samples[i].resize(src_width);
    }

    auto max_comp_value = std::numeric_limits<CompType>::max();

    u32 dst_y = 0;
    for (u32 src_y = 0; src_y < src_height; ++src_y) {
        // Resamplers works on a per line basis with linear data.
        for (u32 src_x = 0; src_x < src_width; ++src_x) {
            for (u32 c = 0; c < num_comps; ++c) {
                f32 value = (f32)src_pixels[(src_y * src_width + src_x) * num_comps + c];
                if constexpr (!is_hdr) {
                    value /= (f32)max_comp_value;
                }

                // Conver to linear space if we are in sRGB and if we are not the alpha component.
                if (is_srgb && c < 3) {
                    samples[c][src_x] = srgb_to_linear(value);
                } else {
                    samples[c][src_x] = value;
                }
            }
        }

        for (u32 c = 0; c < num_comps; ++c) {
            if (!resamplers[c]->put_line(&samples[c][0])) {
                check_resampler_status(*resamplers[c], filter);
            }
        }

        while (true) {
            std::array<const float *, num_comps> output_line{nullptr};
            for (u32 c = 0; c < num_comps; ++c) {
                output_line[c] = resamplers[c]->get_line();
            }

            if (output_line[0] == nullptr) {
                break;  // We retrieved all the ouput lines. Time to place a new source line.
            }

            for (u32 dst_x = 0; dst_x < dst_width; ++dst_x) {
                for (u32 c = 0; c < num_comps; c++) {
                    float linear_value = output_line[c][dst_x];
                    float output_value;

                    if (is_srgb && c < 3) {
                        output_value = linear_to_srgb(linear_value);
                    } else {
                        output_value = linear_value;
                    }

                    if constexpr (is_hdr) {
                        dst_pixels[(dst_y * dst_width + dst_x) * num_comps + c] = output_value;
                    } else {
                        const f32 scaled = output_value * (f32)max_comp_value + 0.5f;
                        const auto unorm_value = std::isnan(scaled) ? CompType{0}
                                                 : scaled < 0.f     ? CompType{0}
                                                 : scaled > max_comp_value
                                                     ? CompType{max_comp_value}
                                                     : static_cast<CompType>(scaled);
                        dst_pixels[(dst_y * dst_width + dst_x) * num_comps + c] = unorm_value;
                    }
                }
            }

            ++dst_y;
        }
    }
}

template <typename T>
static void dump_accessor(std::span<T> &out, const tinygltf::Model &model,
                          const tinygltf::Accessor &accessor) {
    const auto &buffer_view = model.bufferViews[accessor.bufferView];
    const auto &buffer = model.buffers[buffer_view.buffer];
    auto data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
    auto stride = accessor.ByteStride(buffer_view);

    if (buffer_view.byteStride == 0) {
        INFO("Accessor data is tightly packed, using memcpy");
        std::memcpy(out.data(), data, out.size() * sizeof(T));
        return;
    }

    INFO("Accessor data is not tightly packed. Copying with respect to stride");
    for (size_t i = 0; i < accessor.count; i++) {
        out[i] = *(T *)(data + stride * i);
    }
}

template <typename T>
static void dump_accessor_append(std::vector<T> &out, const tinygltf::Model &model,
                                 const tinygltf::Accessor &accessor,
                                 bool allow_size_mismatch = false) {
    assert(accessor.type > 0);
    u32 accessor_comp_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    u32 accessor_num_comp = tinygltf::GetNumComponentsInType(accessor.type);
    u32 accessor_elem_size = accessor_num_comp * accessor_comp_size;
    u32 size_in_bytes = accessor.count * accessor_elem_size;

    if constexpr (verbose_accessor_logging) {
        INFO("------------- Dumping accessor... -------------");
        INFO("Num accessor elems: {}", accessor.count);
        INFO("Accesor comp size: {}", accessor_comp_size);
        INFO("Accesor num comp in elem: {}", accessor_num_comp);
        INFO("Accessor elem size: {}", accessor_elem_size);
        INFO("Accessor size in bytes: {}", size_in_bytes);
        INFO("allow size mismatch?: {}", allow_size_mismatch);
    }

    if (sizeof(T) != accessor_elem_size && !allow_size_mismatch) {
        ERROR("Provided type has size of {} but accessor element size is {}", sizeof(T),
              accessor_elem_size);
        exit(1);
    }

    u32 num_elements = size_in_bytes / sizeof(T);
    u32 start = out.size();

    out.resize(out.size() + num_elements);
    std::span<T> out_span(&out[start], num_elements);

    dump_accessor(out_span, model, accessor);
}

AssetImporter::AssetImporter(std::string exe_path) {
    auto exe_dir = std::filesystem::path(exe_path).parent_path();
    m_cache_dir = exe_dir.concat("/.texture_cache").string();

    if (!std::filesystem::exists(m_cache_dir)) {
        std::filesystem::create_directory(m_cache_dir);
    }

    INFO("Cache dir is {}", m_cache_dir);
}

void AssetImporter::load_asset(std::string path) {
    INFO("Loading {}", path);
    // m_base_node = m_nodes.size();
    m_base_mesh = m_meshes.size();
    m_base_texture = m_textures.size();
    m_base_image = m_images.size();
    m_base_sampler = m_samplers.size();
    m_base_material = m_materials.size();

    tinygltf::Model model;
    tinygltf::TinyGLTF gltf;
    std::string err;
    std::string warn;

    bool res;
    if (path.contains("glb")) {
        res = gltf.LoadBinaryFromFile(&model, &err, &warn, path);
    } else {
        res = gltf.LoadASCIIFromFile(&model, &err, &warn, path);
    }
    if (!warn.empty()) {
        WARN("{}", warn);
    }

    if (!err.empty()) {
        ERROR("{}", err);
    }

    if (!res) {
        ERROR("Failed to parse the given asset file: {}", path);
        return;
    }
    load_meshes(model);
    // load_nodes(model);
    load_materials(model);
    load_textures(model);
}

void AssetImporter::load_meshes(const tinygltf::Model &model) {
    assert(model.meshes.size() != 0);

    for (size_t i = 0; i < model.meshes.size(); i++) {
        const auto &mesh = model.meshes[i];
        if (mesh.name.size() == 0 || mesh.name.empty()) {
            ERROR("Mesh {} has no name", i);
            exit(1);
        }

        u32 prim_index = m_primitives.size();

        for (const auto &prim : mesh.primitives) {
            load_primitive(model, prim);
        }

        m_meshes.push_back({
            .primitive_index = prim_index,
            .num_primitives = (u32)mesh.primitives.size(),
            .node_index = UINT32_MAX,
        });
    }
}

void AssetImporter::load_primitive(const tinygltf::Model &model, const tinygltf::Primitive &prim) {
    assert(prim.indices >= 0);

    u32 base_vertex = m_vertices.size();

    // OpenGL requires that the offsets to index buffers are aligned to
    // their respective index type and so we just align everything to 4 bytes.
    while (m_indices.size() % 4 != 0) {
        m_indices.push_back(0);
    }

    u32 indices_start = m_indices.size();

    const auto &indices_accessor = model.accessors[prim.indices];
    if (indices_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT &&
        indices_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        ERROR("Primitive has invalid index buffer type. Only u32 and u16 are allowed.");
        exit(1);
    }

    dump_accessor_append(m_indices, model, indices_accessor, true);
    load_vertices(model, prim);

    u32 indices_end = m_indices.size();

    m_primitives.push_back({
        .base_vertex = base_vertex,
        .num_vertices = (u32)m_vertices.size() - base_vertex,
        .indices_start = indices_start,
        .indices_end = indices_end,
        .index_type = (u32)indices_accessor.componentType,
        .material_index = m_base_material + (u32)prim.material,
    });
}

void AssetImporter::load_vertices(const tinygltf::Model &model, const tinygltf::Primitive prim) {
    auto find_required = [&](std::string name) {
        auto it = prim.attributes.find(name);
        if (it == prim.attributes.end()) {
            ERROR("Failed to find required attribute {} in primitive", name);
            exit(1);
        }
        return model.accessors[it->second];
    };

    const auto &pos_accessor = find_required("POSITION");
    const auto &normal_accessor = find_required("NORMAL");
    const auto &uv_accessor = find_required("TEXCOORD_0");
    const auto &tangent_accessor = find_required("TANGENT");

    std::vector<glm::vec4> tangent;
    std::vector<glm::vec3> pos;
    std::vector<glm::vec3> normal;
    std::vector<glm::vec2> uv;
    tangent.reserve(tangent_accessor.count);
    tangent.reserve(pos_accessor.count);
    tangent.reserve(normal_accessor.count);
    tangent.reserve(uv_accessor.count);

    dump_accessor_append(pos, model, pos_accessor);
    dump_accessor_append(normal, model, normal_accessor);
    dump_accessor_append(uv, model, uv_accessor);
    dump_accessor_append(tangent, model, tangent_accessor);

    for (size_t i = 0; i < pos.size(); i++) {
        m_vertices.push_back({
            .tangent = tangent[i],
            .pos = pos[i],
            .normal = normal[i],
            .uv = uv[i],
        });
    }
}

// void AssetImporter::load_nodes(const tinygltf::Model &model) {
//     if (model.scenes.size() == 0) {
//         ERROR("glTF model has no scenes!");
//         exit(1);
//     }
//     if (model.defaultScene == -1) {
//         ERROR("glTF model has no default scene");
//         exit(1);
//     }
//
//     const auto &scene = model.scenes[model.defaultScene];
//     for (size_t i = 0; i < scene.nodes.size(); ++i) {
//         u32 our_node_index = m_nodes.size();
//         m_nodes.resize(m_nodes.size() + 1);
//         load_node(model, scene.nodes[i], our_node_index);
//         m_root_nodes.push_back(our_node_index);
//     }
//
//     for (size_t i = 0; i < m_meshes.size(); ++i) {
//         if (m_meshes[i].node_index == UINT32_MAX) {
//             ERROR("Mesh {} is not associated with any node. Bug in asset loader?", i);
//             exit(1);
//         }
//     }
// }

glm::mat4 to_glm(const std::vector<double> &matrix) {
    return glm::mat4(matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5], matrix[6],
                     matrix[7], matrix[8], matrix[9], matrix[10], matrix[11], matrix[12],
                     matrix[13], matrix[14], matrix[15]);
}

// void AssetImporter::load_node(const tinygltf::Model &model, u32 gltf_node_index,
//                               u32 our_node_index) {
//     const auto &gltf_node = model.nodes[gltf_node_index];
//     INFO("--------- Loading glTF node {} ----------", gltf_node_index);
//     INFO("Node has {} children", gltf_node.children.size());
//     if (m_node_map.find(m_base_node + gltf_node_index) != m_node_map.end()) {
//         ERROR(
//             "An attmept was made parse the same gltf node twice. Either the model is malformed or
//             " "there is a bug in our loader!");
//         exit(1);
//     }
//     m_node_map[m_base_node + gltf_node_index] = our_node_index;
//
//     auto &our_node = m_nodes[our_node_index];
//     our_node = {
//         .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
//         .child_index = (u32)m_nodes.size(),
//         .scale = glm::vec3(1.0f),
//         .num_children = (u32)gltf_node.children.size(),
//     };
//
//     if (gltf_node.mesh != -1) {
//         m_meshes[m_base_mesh + gltf_node.mesh].node_index = our_node_index;
//         INFO("Node references mesh {}", gltf_node.mesh);
//     }
//
//     if (gltf_node.scale.size() != 0) {
//         our_node.scale = glm::vec3(gltf_node.scale[0], gltf_node.scale[1], gltf_node.scale[2]);
//     }
//     if (gltf_node.rotation.size() != 0) {
//         our_node.rotation = glm::quat(gltf_node.rotation[3], gltf_node.rotation[0],
//                                       gltf_node.rotation[1], gltf_node.rotation[2]);
//     }
//     if (gltf_node.translation.size() != 0) {
//         our_node.translation =
//             glm::vec3(gltf_node.translation[0], gltf_node.translation[1],
//             gltf_node.translation[2]);
//     }
//
//     if (gltf_node.matrix.size()) {
//         auto mat = to_glm(gltf_node.matrix);
//
//         glm::vec3 skew;
//         glm::vec4 perspective;
//         bool did_decompose = glm::decompose(mat, our_node.scale, our_node.rotation,
//                                             our_node.translation, skew, perspective);
//         assert(did_decompose);
//     }
//
//     // After we resize we can no longer use the alias 'our_node'
//     m_nodes.resize(m_nodes.size() + our_node.num_children);
//
//     for (size_t i = 0; i < m_nodes[our_node_index].num_children; i++) {
//         load_node(model, gltf_node.children[i], m_nodes[our_node_index].child_index + i);
//     }
// }

void AssetImporter::load_textures(const tinygltf::Model &model) {
    std::vector<bool> is_srgb;
    std::vector<bool> is_normal_map;
    determine_required_images(model, is_srgb, is_normal_map);
    load_samplers(model);
    load_images(model, is_srgb, is_normal_map);

    for (size_t i = 0; i < model.textures.size(); ++i) {
        const auto &texture = model.textures[i];

        m_textures.push_back({
            .sampler_index = m_base_sampler + texture.sampler,
            .image_index = m_base_image + texture.source,
        });
    }
}

void AssetImporter::determine_required_images(const tinygltf::Model &model,
                                              std::vector<bool> &is_srgb,
                                              std::vector<bool> &is_normal_map) {
    is_srgb.resize(model.images.size(), false);
    is_normal_map.resize(model.images.size(), false);
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto &material = model.materials[i];
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
            u32 texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
            const auto &tex = model.textures[texture_index];
            is_srgb[tex.source] = true;
        }

        if (material.normalTexture.index != -1) {
            u32 texture_index = material.normalTexture.index;
            const auto &tex = model.textures[texture_index];
            is_normal_map[tex.source] = true;
        }
    }
}

void AssetImporter::load_samplers(const tinygltf::Model &model) {
    constexpr u32 GL_NEAREST = 0x2600;
    constexpr u32 GL_LINEAR = 0x2601;
    constexpr u32 GL_NEAREST_MIPMAP_NEAREST = 0x2700;
    constexpr u32 GL_LINEAR_MIPMAP_NEAREST = 0x2701;
    constexpr u32 GL_NEAREST_MIPMAP_LINEAR = 0x2702;
    constexpr u32 GL_LINEAR_MIPMAP_LINEAR = 0x2703;
    constexpr u32 GL_CLAMP_TO_EDGE = 0x812F;
    constexpr u32 GL_MIRRORED_REPEAT = 0x8370;
    constexpr u32 GL_REPEAT = 0x2901;

    for (size_t i = 0; i < model.samplers.size(); i++) {
        const auto &sampler = model.samplers[i];
        u32 min_filter = sampler.minFilter == -1 ? GL_LINEAR_MIPMAP_LINEAR : sampler.minFilter;
        u32 mag_filter = sampler.magFilter == -1 ? GL_LINEAR : sampler.magFilter;

        auto sampler_filter = [&](u32 filter) {
            switch (filter) {
                case GL_NEAREST_MIPMAP_LINEAR:
                case GL_NEAREST_MIPMAP_NEAREST:
                case GL_NEAREST:
                    return Sampler::Filter::nearest;

                case GL_LINEAR_MIPMAP_LINEAR:
                case GL_LINEAR_MIPMAP_NEAREST:
                case GL_LINEAR:
                    return Sampler::Filter::linear;
                default:
                    assert(0);
            }
            std::unreachable();
        };
        auto sampler_mipmap_mode = [&](u32 filter) {
            switch (filter) {
                case GL_LINEAR_MIPMAP_NEAREST:
                case GL_NEAREST_MIPMAP_NEAREST:
                    return Sampler::MipmapMode::nearest;

                case GL_LINEAR_MIPMAP_LINEAR:
                case GL_NEAREST_MIPMAP_LINEAR:
                case GL_NEAREST:
                case GL_LINEAR:
                    return Sampler::MipmapMode::linear;
                default:
                    assert(0);
            }
            std::unreachable();
        };

        auto sampler_adress_mode = [&](u32 mode) {
            switch (mode) {
                case GL_REPEAT:
                    return Sampler::AddressMode::repeat;
                case GL_CLAMP_TO_EDGE:
                    return Sampler::AddressMode::clamp_to_edge;
                case GL_MIRRORED_REPEAT:
                    return Sampler::AddressMode::mirrored_repeat;
                default:
                    assert(0);
            }
            std::unreachable();
        };

        m_samplers.push_back({
            .mag_filter = sampler_filter(mag_filter),
            .min_filter = sampler_filter(min_filter),
            .mipmap_mode = sampler_mipmap_mode(min_filter),
            .address_mode_u = sampler_adress_mode(sampler.wrapS),
            .address_mode_v = sampler_adress_mode(sampler.wrapT),
            .address_mode_w = sampler_adress_mode(sampler.wrapT),
        });
    }
}

static void gen_mipmaps(u32 width, u32 height, const std::vector<u8> &pixels, ktxTexture2 *texture,
                        bool is_srgb, u32 num_levels) {
    INFO("Generating mipmaps...");
    for (u32 level = 1; level < num_levels; ++level) {
        INFO("Generating level {}", level);
        u32 level_width = width >> level;
        u32 level_height = height >> level;

        size_t offset = 0;
        ktxTexture2_GetImageOffset(texture, level, 0, 0, &offset);
        auto data = ktxTexture_GetData(ktxTexture(texture));
        auto size = ktxTexture_GetImageSize(ktxTexture(texture), level);
        assert(size == level_width * level_height * 4);

        resample<u8, 4>(level_width, level_height, std::span(data + offset, size), width, height,
                        pixels, is_srgb);
    }
    INFO("Generated mipmaps.");
}

void AssetImporter::write_texture_to_image_data(ktxTexture2 *texture) {
    u64 total_compressed_size = 0;
    for (u32 level = 0; level < texture->numLevels; ++level) {
        auto level_size = ktxTexture_GetImageSize(ktxTexture(texture), level);
        total_compressed_size += level_size;
    }

    u64 dst_offset = m_image_data.size();
    m_image_data.resize(m_image_data.size() + total_compressed_size);

    auto data = ktxTexture_GetData(ktxTexture(texture));
    for (u32 level = 0; level < texture->numLevels; ++level) {
        auto level_size = ktxTexture_GetImageSize(ktxTexture(texture), level);
        size_t offset = 0;
        ktxTexture2_GetImageOffset(texture, level, 0, 0, &offset);
        std::memcpy(&m_image_data[dst_offset], data + offset, level_size);
        dst_offset += level_size;
    }
}

void AssetImporter::compress_texture(ktxTexture2 *texture, bool is_normal_map) {
    // Compress and then transcode to BC7
    ktxBasisParams params = {0};
    params.structSize = sizeof(params);
    params.uastc = KTX_TRUE;
    params.qualityLevel = 255;
    params.threadCount = std::thread::hardware_concurrency();
    params.normalMap = is_normal_map;

    INFO("Compressing image...");
    auto result = ktxTexture2_CompressBasisEx(texture, &params);
    if (result != KTX_SUCCESS) {
        ERROR("Failed to compress image data.");
        exit(1);
    }

    if (is_normal_map) {
        result = ktxTexture2_TranscodeBasis(texture, KTX_TTF_BC5_RG, 0);
    } else {
        result = ktxTexture2_TranscodeBasis(texture, KTX_TTF_BC7_RGBA, 0);
    }

    if (result != KTX_SUCCESS) {
        ERROR("Failed to transcode image data.");
        exit(1);
    }
    INFO("Compressed image");
}

void AssetImporter::load_image_data_into_texture(ktxTexture2 *texture,
                                                 const tinygltf::Image &image) {
    size_t offset = 0;
    ktxTexture2_GetImageOffset(texture, 0, 0, 0, &offset);

    auto data = ktxTexture_GetData(ktxTexture(texture));
    auto uncompressed_size = ktxTexture_GetImageSize(ktxTexture(texture), 0);
    assert(uncompressed_size == (u32)image.width * (u32)image.height * 4);
    std::memcpy(data + offset, image.image.data(), uncompressed_size);
}

ktxTexture2 *AssetImporter::write_to_texture_cache(const tinygltf::Image &image, bool is_srgb,
                                                   bool is_normal_map, u32 mip_levels,
                                                   std::string_view path) {
    constexpr u32 VK_FORMAT_R8G8B8A8_UNORM = 37;
    constexpr u32 VK_FORMAT_R8G8B8A8_SRGB = 43;

    constexpr u32 GL_RGBA8 = 0x8058;
    constexpr u32 GL_SRGB8_ALPHA8 = 0x8C43;

    // First create ktx texture for storing the RGBA data.
    ktxTextureCreateInfo create_info = {
        .glInternalformat = is_srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8,
        .vkFormat = is_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
        .pDfd = nullptr,
        .baseWidth = (u32)image.width,
        .baseHeight = (u32)image.height,
        .baseDepth = 1,
        .numDimensions = 2,
        .numLevels = mip_levels,
        .numLayers = 1,
        .numFaces = 1,
        .isArray = KTX_FALSE,
        .generateMipmaps = KTX_FALSE,
    };
    ktxTexture2 *uncompressed;
    auto result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &uncompressed);
    if (result != KTX_SUCCESS) {
        ERROR("Failed to create storage for loaded image.");
        exit(1);
    }

    load_image_data_into_texture(uncompressed, image);
    gen_mipmaps(image.width, image.height, image.image, uncompressed, is_srgb, mip_levels);
    compress_texture(uncompressed, is_normal_map);

    result = ktxTexture2_WriteToNamedFile(uncompressed, path.data());
    if (result != KTX_SUCCESS) {
        ERROR("Failed to write texture to texture cache!");
        exit(1);
    }

    INFO("Saved image in texture cache");
    return uncompressed;
}

std::string AssetImporter::get_image_cache_path(std::span<const u8> image_data,
                                                const ImageInfo &image) {
    auto texture_hash = hash_fnv1a(image_data);
    auto metadata = std::span((u8 *)&image, sizeof(ImageInfo));
    for (u8 byte : metadata) {
        update_fnv1a_hash(&texture_hash, byte);
    }

    auto hash_in_hex = std::format("{:016x}", texture_hash);
    auto path = m_cache_dir + "/" + hash_in_hex;
    return path;
}

void AssetImporter::load_images(const tinygltf::Model &model, const std::vector<bool> &is_srgb,
                                const std::vector<bool> &is_normal_map) {
    for (size_t i = 0; i < model.images.size(); ++i) {
        const auto &image = model.images[i];
        u32 width = image.width;
        u32 height = image.height;

        u32 max_dim = std::max(width, height);
        u32 mip_levels = (std::log2(max_dim) + 1);

        ImageInfo::Format format = ImageInfo::Format::BC7_RGBA;
        if (is_srgb[i]) {
            assert(!is_normal_map[i]);
            format = ImageInfo::Format::BC7_RGBA_SRGB;
        }

        if (is_normal_map[i]) {
            format = ImageInfo::Format::BC5_RG;
        }

        // Do this because we need padding bytes to have a consistent value for the hashing.
        ImageInfo our_image;
        static_assert(std::is_trivial<ImageInfo>::value);
        static_assert(std::is_standard_layout<ImageInfo>::value);
        std::memset(&our_image, 0, sizeof(ImageInfo));

        our_image.format = format;
        our_image.width = width;
        our_image.height = height;
        our_image.num_levels = mip_levels;
        our_image.num_faces = 1;
        our_image.is_compressed = true;
        our_image.is_cubemap = false;
        our_image.image_data_index = m_image_data.size();
        m_images.push_back(our_image);

        auto path = get_image_cache_path(image.image, our_image);
        ktxTexture2 *texture;
        if (std::filesystem::exists(path)) {
            INFO("Found image in texture cache");
            auto result = ktxTexture2_CreateFromNamedFile(
                path.c_str(), KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
            if (result != KTX_SUCCESS) {
                ERROR("Image was found in the cache but we failed to read it?");
                exit(1);
            }
        } else {
            texture = write_to_texture_cache(image, is_srgb[i], is_normal_map[i], mip_levels, path);
        }

        write_texture_to_image_data(texture);
        ktxTexture2_Destroy(texture);
    }
}

void AssetImporter::load_materials(const tinygltf::Model &model) {
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto &material = model.materials[i];
        const auto &base_color_factor = material.pbrMetallicRoughness.baseColorFactor;
        const auto &emissive_factor = material.emissiveFactor;

        u32 flags = 0;
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
            flags |= (u32)Material::Flags::has_base_color_texture;
        }

        if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
            flags |= (u32)Material::Flags::has_metallic_roughness_texture;
        }

        if (material.normalTexture.index != -1) {
            flags |= (u32)Material::Flags::has_normal_map;
        }
        if (material.occlusionTexture.index != -1) {
            flags |= (u32)Material::Flags::has_occlusion_map;
        }
        if (material.emissiveTexture.index != -1) {
            flags |= (u32)Material::Flags::has_emission_map;
        }

        if (material.doubleSided) {
            WARN("glTF model has double sided material which will not be rendered correctly");
        }

        m_materials.push_back({
            .flags = (Material::Flags)flags,
            .base_color_factor = glm::vec4(base_color_factor[0], base_color_factor[1],
                                           base_color_factor[2], base_color_factor[3]),
            .metallic_factor = (f32)material.pbrMetallicRoughness.metallicFactor,
            .roughness_factor = (f32)material.pbrMetallicRoughness.roughnessFactor,
            .base_color_texture =
                m_base_texture + material.pbrMetallicRoughness.baseColorTexture.index,
            .metallic_roughness_texture =
                m_base_texture + material.pbrMetallicRoughness.metallicRoughnessTexture.index,
            .normal_map = m_base_texture + material.normalTexture.index,
            .normal_map_scale = (f32)material.normalTexture.scale,
            .occlusion_map = m_base_texture + material.occlusionTexture.index,
            .occlusion_strength = (f32)material.occlusionTexture.strength,
            .emission_map = m_base_texture + material.emissiveTexture.index,
            .emission_factor =
                glm::vec3(emissive_factor[0], emissive_factor[1], emissive_factor[1]),
        });
    }
}

void AssetImporter::load_prefab(std::span<const ImmutableNode> nodes) {
    u32 base_node = m_prefabs_nodes.size();
    m_root_prefab_nodes.push_back(base_node);

    for (const auto &node : nodes) {
        ImmutableNode new_node = node;
        new_node.child_index += base_node;
        m_prefabs_nodes.push_back(new_node);
    }
}