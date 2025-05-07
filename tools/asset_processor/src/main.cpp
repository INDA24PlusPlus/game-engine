#include <fstream>
#include <json.hpp>
#include <print>
#include <unordered_map>

#include "../../../src/engine/scene/Node.h"
#include "../../../src/engine/utils/logging.h"
#include "AssetImporter.h"

template <typename T>
void write_data(std::vector<T>& data, std::ofstream& stream, u32& num_bytes_written) {
    size_t size = sizeof(T) * data.size();
    stream.write((const char*)data.data(), size);
    num_bytes_written += size;
}

using json = nlohmann::json;

static void parse_prefabs(AssetImporter& importer, AssetManifest& manifest,
                          std::unordered_map<std::string, u32>& mesh_names_to_indices,
                          json& prefabs) {
    for (const auto& prefab_path : prefabs) {
        if (!prefab_path.is_string()) {
            ERROR("Prefab must be path to prefab file!");
            exit(1);
        }

        std::string path = prefab_path;

        std::ifstream file(path);
        if (!file.is_open()) {
            FATAL("Failed to open prefab file {}", path);
        }

        json prefab = json::parse(file, nullptr, false);
        if (prefab.is_discarded()) {
            FATAL("Failed to parse json of prefab file {}", path);
        }

        if (!prefab.contains("name")) {
            FATAL("Prefab {} has no name", path);
        }

        if (!prefab.contains("nodes")) {
            FATAL("Prefab {} has no nodes", path);
        }
        if (!prefab.contains("root")) {
            FATAL("Prefab {} has no specified root node", path);
        }
        u32 root_node = prefab["root"];

        std::string prefab_name = prefab["name"];
        manifest.m_prefab_names.push_back(manifest.create_name(prefab_name));

        NodeHierarchy nodes;
        for (const auto& json_node : prefab["nodes"]) {
            Node node;
            std::vector<f32> translation = json_node["translation"];
            std::vector<f32> rotation = json_node["rotation"];
            std::vector<f32> scale = json_node["scale"];

            std::string node_name = json_node["name"];
            node.name = node_name;
            node.translation = glm::vec3(translation[0], translation[1], translation[2]);
            node.rotation = glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]);
            node.scale = glm::vec3(scale[0], scale[1], scale[2]);

            if (json_node.contains("mesh")) {
                std::string mesh_name = json_node["mesh"];
                if (mesh_names_to_indices.find(mesh_name) == mesh_names_to_indices.end()) {
                    FATAL(
                        "Node {} in prefab {} specifies mesh name {} which is not declared in "
                        "manifest file",
                        node.name, path, mesh_name);
                }
                node.kind = Node::Kind::mesh;
                node.mesh_index = mesh_names_to_indices[mesh_name];
                INFO("Node contains mesh");
            }

            if (json_node.contains("children")) {
                std::vector<u32> children = json_node["children"];
                node.children = children;
            }

            nodes.m_nodes.push_back(node);
        }

        const auto& immutable = nodes.to_immutable(manifest, NodeHandle(root_node));
        importer.load_prefab(immutable);
    }
}

int main(int argc, const char** argv) {
    if (argc != 2) {
        std::println("Usage {} <manifest_file>", argv[0]);
        exit(1);
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        ERROR("Failed to open file manifest file {}", argv[1]);
        exit(1);
    }

    json manifest = json::parse(file, nullptr, false);
    if (manifest.is_discarded()) {
        INFO("Failed to parse json!");
        exit(1);
    }

    if (!manifest.contains("meshes")) {
        ERROR("Manifest file contains no mesh array!");
        exit(1);
    }
    if (!manifest["meshes"].is_array()) {
        ERROR("\"meshes\" is not an array");
        exit(1);
    }

    AssetImporter importer(argv[0]);
    AssetManifest engine_manifest;
    std::unordered_map<std::string, u32> mesh_names_to_indices;

    for (const auto& mesh : manifest["meshes"]) {
        if (!mesh.contains("name")) {
            ERROR("Mesh must have a name associated with it");
            exit(1);
        }
        if (!mesh.contains("path")) {
            ERROR("Mesh must have a path associated with it");
            exit(1);
        }

        std::string name = mesh["name"];
        std::string path = mesh["path"];
        if (name.length() > 255) {
            ERROR("The maximum allowed length of a mesh name is 255");
            exit(1);
        }

        importer.load_asset(path);
        engine_manifest.m_mesh_names.push_back(engine_manifest.create_name(name));
        mesh_names_to_indices[name] = importer.m_meshes.size() - 1;
    }

    if (manifest.contains("prefabs")) {
        parse_prefabs(importer, engine_manifest, mesh_names_to_indices, manifest["prefabs"]);
    }

    constexpr u32 curr_header_version = 2;

    AssetHeader header;
    header.version = curr_header_version;
    header.num_indices = importer.m_indices.size();
    header.num_vertices = importer.m_vertices.size();
    header.num_meshes = importer.m_meshes.size();
    header.num_primitives = importer.m_primitives.size();
    header.num_prefab_nodes = importer.m_prefabs_nodes.size();
    header.num_samplers = importer.m_samplers.size();
    header.num_images = importer.m_images.size();
    header.num_textures = importer.m_textures.size();
    header.num_materials = importer.m_materials.size();
    header.num_prefabs = engine_manifest.m_prefab_names.size();
    header.num_name_bytes = engine_manifest.m_name_bytes.size();
    header.num_image_bytes = importer.m_image_data.size();

    std::ofstream out_file("scene_data.bin", std::ios::binary);

    // Write header.
    out_file.write((const char*)&header, sizeof(AssetHeader));
    u32 num_bytes_written = 0;
    write_data(importer.m_indices, out_file, num_bytes_written);
    write_data(importer.m_vertices, out_file, num_bytes_written);
    write_data(importer.m_meshes, out_file, num_bytes_written);
    write_data(importer.m_primitives, out_file, num_bytes_written);
    write_data(importer.m_prefabs_nodes, out_file, num_bytes_written);
    write_data(importer.m_root_prefab_nodes, out_file, num_bytes_written);
    write_data(importer.m_samplers, out_file, num_bytes_written);
    write_data(importer.m_images, out_file, num_bytes_written);
    write_data(importer.m_textures, out_file, num_bytes_written);
    write_data(importer.m_materials, out_file, num_bytes_written);
    // write_data(engine_manifest.m_name_bytes, out_file, num_bytes_written);
    // write_data(engine_manifest.m_mesh_names, out_file, num_bytes_written);
    engine_manifest.serialize(out_file, num_bytes_written);
    write_data(importer.m_image_data, out_file, num_bytes_written);
    INFO("wrote {} bytes", num_bytes_written + sizeof(AssetHeader));

    out_file.flush();
    out_file.close();
}
