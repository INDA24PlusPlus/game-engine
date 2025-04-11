#include "AssetImporter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <tiny_gltf.h>
#include <print>

#include "glm/fwd.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
