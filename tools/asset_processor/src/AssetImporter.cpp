#include "AssetImporter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <tiny_gltf.h>
#include <stb_image.h>

#include "glm/fwd.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../../../src/engine/utils/logging.h"

void AssetImporter::load_asset(std::string path) {
    m_base_mesh = m_meshes.size();
    m_base_root_node = m_root_nodes.size();

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
    INFO("Successfully parsed asset file {}", path);

    INFO("Beginning extraction process...");
    load_meshes(model);
    INFO("Successfully extracted mesh data.");

    u32 base_node = load_nodes(model);
    // For later...
    (void)base_node;
    INFO("Loaded nodes");
}

void AssetImporter::load_meshes(const tinygltf::Model& model) {
    INFO("GLTF model has {} meshes", model.meshes.size());

    for (size_t i = 0; i < model.meshes.size(); i++) {
        const auto& mesh = model.meshes[i];
        INFO("Extracting mesh {} with {} primitives", mesh.name, mesh.primitives.size());

        u32 prim_index = m_primitives.size();

        for (const auto& prim : mesh.primitives) {
            load_primitive(model, prim);
        }

        m_meshes.push_back({
            .primitive_index = prim_index,
            .num_primitives = (u32)m_primitives.size() - prim_index,
            .node_index = UINT32_MAX,
        });

        m_mesh_name_indices.push_back(m_name_data.size());
        std::string name = mesh.name;

        if (mesh.name.size() == 0) {
            name = "untitled" + std::to_string(m_meshes.size());
        }

        for (size_t i = 0; i < mesh.name.size(); i++) {
            m_name_data.push_back(name[i]);
        }
    }
}

void AssetImporter::load_primitive(const tinygltf::Model& model, const tinygltf::Primitive& prim) {
    assert(prim.indices >= 0);

    u32 base_vertex = m_vertices.size();
    u32 base_index = m_indices.size();

    const auto& indices_accessor = model.accessors[prim.indices];
    load_indices(model, indices_accessor);
    load_vertices(model, prim);

    // We added no static vertices, must be skinned primitive.
    m_primitives.push_back({
        .base_vertex = base_vertex,
        .num_vertices = (u32)m_vertices.size() - base_vertex,
        .base_index = base_index,
        .num_indices = (u32)m_indices.size() - base_index,
    });
}

glm::mat4 to_glm(const std::vector<double>& matrix) {
    return glm::mat4(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]
    );
}

u32 AssetImporter::load_nodes(const tinygltf::Model& model) {
    const auto& scene = model.scenes[model.defaultScene];
    u32 base_node = m_nodes.size();
    m_nodes.reserve(base_node + model.nodes.size());
    m_nodes.resize(base_node + 1);

    m_root_nodes.push_back(m_base_root_node + 0);
    load_node(base_node, 0, scene.nodes[0], model.nodes);
    for (size_t i = 1; i < scene.nodes.size(); ++i) {
        u32 root_node_index = m_nodes.size();
        m_nodes.push_back({});
        m_root_nodes.push_back(root_node_index);
        load_node(base_node, root_node_index - base_node, scene.nodes[i], model.nodes);
    }

    return base_node;
}

void AssetImporter::load_node(u32 base_node, u32 node_index, u32 gltf_node_index, std::span<const tinygltf::Node> nodes) {
    m_node_map[base_node + gltf_node_index] = base_node + node_index;
    const auto& gltf_node = nodes[gltf_node_index];

    auto& our_node = m_nodes[base_node + node_index];
    our_node = {
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .child_index = (u32)m_nodes.size(),
        .scale = glm::vec3(1.0f),
        .num_children = (u32)gltf_node.children.size(),
    };

    if (gltf_node.mesh != -1) {
        m_meshes[m_base_mesh + gltf_node.mesh].node_index = base_node + node_index;
    }

    if (gltf_node.scale.size() != 0) {
        our_node.scale = glm::vec3(
            gltf_node.scale[0],
            gltf_node.scale[1],
            gltf_node.scale[2]
        );
    }
    if (gltf_node.rotation.size() != 0) {
        our_node.rotation = glm::quat(
            gltf_node.rotation[3],
            gltf_node.rotation[0],
            gltf_node.rotation[1],
            gltf_node.rotation[2]
        );
    }
    if (gltf_node.translation.size() != 0) {
        our_node.translation = glm::vec3(
            gltf_node.translation[0],
            gltf_node.translation[1],
            gltf_node.translation[2]
        );
    }

    if (gltf_node.matrix.size()) {
        DEBUG("Has matrix, skin is {}", gltf_node.skin);
        auto mat = to_glm(gltf_node.matrix);

        glm::vec3 skew;
        glm::vec4 perspective;
        bool did_decompose = glm::decompose(mat, our_node.scale, our_node.rotation, our_node.translation, skew, perspective);
        assert(did_decompose);
    }

    m_nodes.resize(m_nodes.size() + our_node.num_children);
    for (size_t i = 0; i < our_node.num_children; i++) {
        load_node(base_node, our_node.child_index + i, gltf_node.children[i], nodes);
    }
}

void AssetImporter::load_vertices(const tinygltf::Model& model, const tinygltf::Primitive prim) {
    assert(prim.attributes.find("POSITION") != prim.attributes.end());
    assert(prim.attributes.find("NORMAL") != prim.attributes.end());

    auto base_vertex = m_vertices.size();

    const auto& pos_accessor = model.accessors[prim.attributes.at("POSITION")];
    const auto& normal_accessor = model.accessors[prim.attributes.at("NORMAL")];
    m_vertices.resize(base_vertex + pos_accessor.count);

    AccessorIterator it;
    it.init(model, pos_accessor);

    glm::vec3 pos;
    while (it.next(pos)) {
        m_vertices[base_vertex + it.index - 1].pos = pos;
    }

    it.init(model, normal_accessor);
    glm::vec3 normal;
    while (it.next(normal)) {
        m_vertices[base_vertex + it.index - 1].normal = normal;
    }

    if (prim.attributes.find("TEXCOORD_1") != prim.attributes.end()) {
        WARN("Current GLTF file has more then one set of texture coordinates. I currently only support one set of texture coordinates. Contact Vidar if you really need this changed");
    }


    if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end()) {
        const auto& uv_accessor = model.accessors[prim.attributes.at("TEXCOORD_0")];
        it.init(model, uv_accessor);
        glm::vec2 uv;
        while (it.next(uv)) {
            m_vertices[base_vertex + it.index - 1].uv = uv;
        }
    } else WARN("No suitable texture coordinates found on primtive. Using garbage values.");
}

void AssetImporter::load_indices(const tinygltf::Model& model, const tinygltf::Accessor accessor) {
    auto base_index = m_indices.size();
    m_indices.resize(m_indices.size() + accessor.count);

    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        AccessorIterator it;
        it.init(model, accessor);

        u16 value;
        while (it.next(value)) {
            m_indices[base_index + it.index - 1] = value;
        }
    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        AccessorIterator it;
        it.init(model, accessor);
        u32 value;
        while (it.next(value)) {
            m_indices[base_index + it.index - 1] = value;
        }
    } else assert(0);
}

void AccessorIterator::init(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    const auto& buffer_view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[buffer_view.buffer];
    data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
    stride = accessor.ByteStride(buffer_view);
    count = accessor.count;
    index = 0;
}
