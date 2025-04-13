#include "AssetImporter.h"

#include <ktx.h>
#include <tiny_gltf.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <print>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "encoder/basisu_resampler.h"
#include "glm/fwd.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "../../../src/engine/utils/logging.h"

template <typename T>
static void dump_accessor(std::span<T>& out, const tinygltf::Model& model,
                          const tinygltf::Accessor& accessor) {
    const auto& buffer_view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[buffer_view.buffer];
    auto data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
    auto stride = accessor.ByteStride(buffer_view);

    if (buffer_view.byteStride == 0) {
        INFO("Accessor data is tightly packed, using memcpy");
        std::memcpy(out.data(), data, out.size() * sizeof(T));
        return;
    }

    INFO("Accessor data is not tightly packed. Copying with respect to stride");
    for (size_t i = 0; i < accessor.count; i++) {
        out[i] = *(T*)(data + stride * i);
    }
}

template <typename T>
static void dump_accessor_append(std::vector<T>& out, const tinygltf::Model& model,
                                 const tinygltf::Accessor& accessor,
                                 bool allow_size_mismatch = false) {
    INFO("------------- Dumping accessor... -------------");
    assert(accessor.type > 0);
    u32 accessor_comp_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    u32 accessor_num_comp = tinygltf::GetNumComponentsInType(accessor.type);
    u32 accessor_elem_size = accessor_num_comp * accessor_comp_size;
    u32 size_in_bytes = accessor.count * accessor_elem_size;

    INFO("Num accessor elems: {}", accessor.count);
    INFO("Accesor comp size: {}", accessor_comp_size);
    INFO("Accesor num comp in elem: {}", accessor_num_comp);
    INFO("Accessor elem size: {}", accessor_elem_size);
    INFO("Accessor size in bytes: {}", size_in_bytes);

    INFO("allow size mismatch?: {}", allow_size_mismatch);
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
    INFO("------------- Dumped accessor -------------");
}

void AssetImporter::load_asset(std::string path) {
    m_base_node = m_nodes.size();
    m_base_mesh = m_meshes.size();
    m_base_texture = m_textures.size();
    m_base_image = m_images.size();
    m_base_sampler = m_samplers.size();
    m_base_material = m_materials.size();

    tinygltf::Model model;
    tinygltf::TinyGLTF gltf;
    std::string err;
    std::string warn;

    bool res = gltf.LoadASCIIFromFile(&model, &err, &warn, path);
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
    load_nodes(model);
    load_materials(model);
    load_textures(model);
}

void AssetImporter::load_meshes(const tinygltf::Model& model) {
    assert(model.meshes.size() != 0);

    for (size_t i = 0; i < model.meshes.size(); i++) {
        const auto& mesh = model.meshes[i];
        if (mesh.name.size() == 0 || mesh.name.empty()) {
            ERROR("Mesh {} has no name", i);
            exit(1);
        }

        if (m_mesh_names.find(mesh.name) != m_mesh_names.end()) {
            ERROR("Mesh name {} is already used.", mesh.name);
            exit(1);
        }

        m_mesh_names[mesh.name] = m_meshes.size();
        u32 prim_index = m_primitives.size();

        for (const auto& prim : mesh.primitives) {
            load_primitive(model, prim);
        }

        m_meshes.push_back({
            .primitive_index = prim_index,
            .num_primitives = (u32)mesh.primitives.size(),
            .node_index = UINT32_MAX,
        });

        INFO("Parsed mesh");
    }
}

void AssetImporter::load_primitive(const tinygltf::Model& model, const tinygltf::Primitive& prim) {
    assert(prim.indices >= 0);

    u32 base_vertex = m_vertices.size();

    // OpenGL requires that the offsets to index buffers are aligned to
    // their respective index type and so we just align everything to 4 bytes.
    while (m_indices.size() % 4 != 0) {
        m_indices.push_back(0);
    }

    u32 indices_start = m_indices.size();

    const auto& indices_accessor = model.accessors[prim.indices];
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
    INFO("Parsed primitive");
}

void AssetImporter::load_vertices(const tinygltf::Model& model, const tinygltf::Primitive prim) {
    if (prim.attributes.find("POSITION") == prim.attributes.end()) {
        ERROR("Primitive is missing POSITION attribute");
        exit(1);
    }
    if (prim.attributes.find("NORMAL") == prim.attributes.end()) {
        ERROR("Primitive is missing NORMAL attribute");
        exit(1);
    }
    if (prim.attributes.find("TEXCOORD_0") == prim.attributes.end()) {
        ERROR("Primitive is missing TEXCOORD_0 attribute");
        exit(1);
    }

    const auto& pos_accessor = model.accessors[prim.attributes.at("POSITION")];
    const auto& normal_accessor = model.accessors[prim.attributes.at("NORMAL")];
    const auto& uv_accessor = model.accessors[prim.attributes.at("TEXCOORD_0")];

    std::vector<glm::vec3> pos;
    pos.reserve(pos_accessor.count);
    std::vector<glm::vec3> normal;
    normal.reserve(normal_accessor.count);
    std::vector<glm::vec2> uv;
    uv.reserve(uv_accessor.count);

    dump_accessor_append(pos, model, pos_accessor);
    dump_accessor_append(normal, model, normal_accessor);
    dump_accessor_append(uv, model, uv_accessor);

    // glTF spec mandates this, but just to make sure.
    assert(pos.size() == normal.size() && pos.size() == uv.size());
    for (size_t i = 0; i < pos.size(); i++) {
        m_vertices.push_back({
            .pos = pos[i],
            .normal = normal[i],
            .uv = uv[i],
        });
    }
}

void AssetImporter::load_nodes(const tinygltf::Model& model) {
    if (model.scenes.size() == 0) {
        ERROR("glTF model has no scenes!");
        exit(1);
    }
    if (model.defaultScene == -1) {
        ERROR("glTF model has no default scene");
        exit(1);
    }

    const auto& scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        u32 our_node_index = m_nodes.size();
        m_nodes.resize(m_nodes.size() + 1);
        load_node(model, scene.nodes[i], our_node_index);
        m_root_nodes.push_back(our_node_index);
    }

    for (size_t i = 0; i < m_mesh_names.size(); ++i) {
        if (m_meshes[i].node_index == UINT32_MAX) {
            ERROR("Mesh {} is not associated with any node. Bug in asset loader?", i);
            exit(1);
        }
    }
}

glm::mat4 to_glm(const std::vector<double>& matrix) {
    return glm::mat4(matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5], matrix[6],
                     matrix[7], matrix[8], matrix[9], matrix[10], matrix[11], matrix[12],
                     matrix[13], matrix[14], matrix[15]);
}

void AssetImporter::load_node(const tinygltf::Model& model, u32 gltf_node_index,
                              u32 our_node_index) {
    const auto& gltf_node = model.nodes[gltf_node_index];
    INFO("--------- Loading glTF node {} ----------", gltf_node_index);
    INFO("Node has {} children", gltf_node.children.size());
    if (m_node_map.find(m_base_node + gltf_node_index) != m_node_map.end()) {
        ERROR(
            "An attmept was made parse the same gltf node twice. Either the model is malformed or "
            "there is a bug in our loader!");
        exit(1);
    }
    m_node_map[m_base_node + gltf_node_index] = our_node_index;

    auto& our_node = m_nodes[our_node_index];
    our_node = {
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .child_index = (u32)m_nodes.size(),
        .scale = glm::vec3(1.0f),
        .num_children = (u32)gltf_node.children.size(),
    };

    if (gltf_node.mesh != -1) {
        m_meshes[m_base_mesh + gltf_node.mesh].node_index = our_node_index;
        INFO("Node references mesh {}", gltf_node.mesh);
    }

    if (gltf_node.scale.size() != 0) {
        our_node.scale = glm::vec3(gltf_node.scale[0], gltf_node.scale[1], gltf_node.scale[2]);
    }
    if (gltf_node.rotation.size() != 0) {
        our_node.rotation = glm::quat(gltf_node.rotation[3], gltf_node.rotation[0],
                                      gltf_node.rotation[1], gltf_node.rotation[2]);
    }
    if (gltf_node.translation.size() != 0) {
        our_node.translation =
            glm::vec3(gltf_node.translation[0], gltf_node.translation[1], gltf_node.translation[2]);
    }

    if (gltf_node.matrix.size()) {
        auto mat = to_glm(gltf_node.matrix);

        glm::vec3 skew;
        glm::vec4 perspective;
        bool did_decompose = glm::decompose(mat, our_node.scale, our_node.rotation,
                                            our_node.translation, skew, perspective);
        assert(did_decompose);
    }

    // After we resize we can no longer use the alias 'our_node'
    m_nodes.resize(m_nodes.size() + our_node.num_children);

    for (size_t i = 0; i < m_nodes[our_node_index].num_children; i++) {
        load_node(model, gltf_node.children[i], m_nodes[our_node_index].child_index + i);
    }
}

void AssetImporter::load_textures(const tinygltf::Model& model) {
    std::vector<bool> is_srgb;
    determine_required_images(model, is_srgb);
    load_samplers(model);
    load_images(model, is_srgb);

    for (size_t i = 0; i < model.textures.size(); ++i) {
        const auto& texture = model.textures[i];

        m_textures.push_back({
            .sampler_index = m_base_sampler + texture.sampler,
            .image_index = m_base_image + texture.source,
        });
    }
}

void AssetImporter::determine_required_images(const tinygltf::Model& model,
                                              std::vector<bool>& is_srgb) {
    is_srgb.resize(model.images.size(), false);
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto& material = model.materials[i];
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
            u32 texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
            const auto& tex = model.textures[texture_index];
            is_srgb[tex.source] = true;
        }
    }
}

void AssetImporter::load_samplers(const tinygltf::Model& model) {
    constexpr u32 GL_LINEAR_MIPMAP_LINEAR = 0x2703;
    constexpr u32 GL_LINEAR = 0x2601;

    for (size_t i = 0; i < model.samplers.size(); i++) {
        const auto& sampler = model.samplers[i];
        u32 min_filter = sampler.minFilter == -1 ? GL_LINEAR_MIPMAP_LINEAR : sampler.minFilter;
        u32 mag_filter = sampler.magFilter == -1 ? GL_LINEAR : sampler.magFilter;
        m_samplers.push_back({
            .min_filter = min_filter,
            .mag_filter = mag_filter,
            .wrap_t = (u32)sampler.wrapT,
            .wrap_s = (u32)sampler.wrapS,
        });
    }
}

void AssetImporter::load_images(const tinygltf::Model& model, const std::vector<bool>& is_srgb) {
    for (size_t i = 0; i < model.images.size(); ++i) {
        const auto& image = model.images[i];
        m_images.push_back({
            .width = (u32)image.width,
            .height = (u32)image.height,
            .is_srb = is_srgb[i],
            .image_data_index = m_image_data.size(),
        });

        constexpr u32 VK_FORMAT_R8G8B8A8_UNORM = 37;
        constexpr u32 VK_FORMAT_R8G8B8A8_SRGB = 43;

        constexpr u32 GL_RGBA8 = 0x8058;
        constexpr u32 GL_SRGB8_ALPHA8 = 0x8C43;

        // First create ktx texture for storing the RGBA data.
        ktxTextureCreateInfo create_info = {
            .glInternalformat = is_srgb[i] ? GL_SRGB8_ALPHA8 : GL_RGBA8,
            .vkFormat = is_srgb[i] ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
            .pDfd = nullptr,
            .baseWidth = (u32)image.width,
            .baseHeight = (u32)image.height,
            .baseDepth = 1,
            .numDimensions = 2,
            .numLevels = 1,
            .numLayers = 1,
            .numFaces = 1,
            .isArray = KTX_FALSE,
            .generateMipmaps = KTX_FALSE,
        };
        ktxTexture2* uncompressed;
        auto result =
            ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &uncompressed);
        if (result != KTX_SUCCESS) {
            ERROR("Failed to create storage for loaded image.");
            exit(1);
        }

        size_t offset = 0;
        ktxTexture2_GetImageOffset(uncompressed, 0, 0, 0, &offset);

        auto data = ktxTexture_GetData(ktxTexture(uncompressed));
        auto uncompressed_size = ktxTexture_GetImageSize(ktxTexture(uncompressed), 0);

        assert(uncompressed_size == (u32)image.width * (u32)image.height * 4);
        std::memcpy(data + offset, image.image.data(), uncompressed_size);

        // Compress and then transcode to BC7
        ktxBasisParams params = {0};
        params.structSize = sizeof(params);
        params.uastc = KTX_TRUE;
        params.qualityLevel = 255;
        params.threadCount = std::thread::hardware_concurrency();

        INFO("Compressing image");
        result = ktxTexture2_CompressBasisEx(uncompressed, &params);
        if (result != KTX_SUCCESS) {
            ERROR("Failed to compress image data.");
            exit(1);
        }

        result = ktxTexture2_TranscodeBasis(uncompressed, KTX_TTF_BC7_RGBA, 0);
        if (result != KTX_SUCCESS) {
            ERROR("Failed to transcode image data to BC7.");
            exit(1);
        }

        size_t compressed_offset = 0;
        ktxTexture2_GetImageOffset(uncompressed, 0, 0, 0, &compressed_offset);
        auto compressed_size = ktxTexture_GetImageSize(ktxTexture(uncompressed), 0);

        size_t dst_offset = m_image_data.size();
        m_image_data.resize(m_image_data.size() + compressed_size);

        std::memcpy(&m_image_data[dst_offset],
                    ktxTexture_GetData(ktxTexture(uncompressed)) + compressed_offset,
                    compressed_size);

        m_images[m_images.size() - 1].image_size = compressed_size;
        ktxTexture2_Destroy(uncompressed);
        INFO("Compressed image");
    }
}

void AssetImporter::load_materials(const tinygltf::Model& model) {
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto& material = model.materials[i];
        const auto& base_color_factor = material.pbrMetallicRoughness.baseColorFactor;

        u32 flags = 0;
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
            flags |= (u32)Material::Flags::has_base_color_texture;
        }

        if (material.doubleSided) {
            WARN("glTF model has double sided material which will not be rendered correctly");
        }

        m_materials.push_back({
            .flags = (Material::Flags)flags,
            .base_color_texture =
                m_base_texture + material.pbrMetallicRoughness.baseColorTexture.index,
        });
    }
}

// What we need from image:
// width, height, pixels

static f32 srgb_to_linear(f32 srgb) {
    return srgb <= 0.04045f ? srgb * (1.0f / 12.92f)
                            : std::pow((srgb + 0.055f) * (1.0f / 1.055f), 2.4f);
}

static f32 linear_to_srgb(f32 linear) {
    return linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

static void check_resampler_status(basisu::Resampler& resampler, const char* filter) {
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

template <typename CompType, u32 num_comps>
static void resample(u32 dst_width, u32 dst_height, std::span<CompType> dst_pixels, u32 src_width,
                     u32 src_height, std::span<CompType> src_pixels, bool is_srgb) {
    if (std::max(src_width, src_height) > BASISU_RESAMPLER_MAX_DIMENSION ||
        std::max(dst_width, dst_height)) {
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
        resamplers[i] = std::make_unique(src_width, src_height, dst_width, dst_height, wrap_mode,
                                         0.0f, is_hdr ? 0.0f : 1.0f, filter,
                                         i == 0 ? nullptr : resamplers[0]->get_clist_x(),
                                         i == 0 ? nullptr : resamplers[0]->get_clist_y(),
                                         filter_scale, filter_scale, 0.0f, 0.0f);
        check_resampler_status(*resamplers[i], filter);
        samples[i].resize(src_width);
    }

    auto max_comp_value = std::numeric_limits<CompType>::max();

    u32 dst_y = 0;
    for (u32 src_y = 0; src_y < src_height; ++src_y) {
        // Resamplers works on a per line basis with linear data.
        for (u32 src_x; src_x < src_width; ++src_x) {
            for (u32 c = 0; c < num_comps; ++c) {
                f32 value = (f32)src_pixels[(src_y * src_width + src_x) * num_comps + c];
                if constexpr (is_hdr) {
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
            if (!resamplers[c]->put_line(*samples[c][0], filter)) {
                check_sampler_status(*resamplers[c], filter);
            }
        }

        while (true) {
            std::array<const float*, num_comps> output_line{nullptr};
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

// From KTX 2.0 tools/toktx/toktx.cc
// void genMipmap(Image* image, u32 layer, u32 face_slice, ktxTexture* texture, bool normalize) {
//    std::unique_ptr<Image> level_image;
//    for (uint32_t glevel = 1; glevel < texture->numLevels; glevel++) {
//        auto level_width = maximum<uint32_t>(1, image->getWidth() >> glevel);
//        auto level_height = maximum<uint32_t>(1, image->getHeight() >> glevel);
//        try {
//            // Default settings from the toktx tool.
//            auto filter = "lanczos4";
//            f32 filter_scale = 1.0f;
//            auto wrap_mode = basisu::Resampler::Boundary_Op::BOUNDARY_CLAMP;
//            level_image =
//                image->resample(level_width, level_height, filter, filter_scale, wrap_mode);
//        } catch (std::runtime_error& e) {
//            ERROR("Image resampling failed while generating mipmaps for textures! {}", e.what());
//            exit(1);
//        }
//
//        // For normal maps, I think?
//        if (normalize) {
//            level_image->normalize();
//        }
//
//        MAYBE_UNUSED ktx_error_code_e ret;
//        ret = ktxTexture_SetImageFromMemory(texture, glevel, layer, face_slice, *level_image,
//                                            level_image->getByteCount());
//        assert(ret == KTX_SUCCESS);
//    }
//}