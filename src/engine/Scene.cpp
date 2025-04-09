#include "Scene.h"
#include <cstdint>
#include <fstream>

#include "utils/logging.h"

namespace engine {

template<typename T>
static std::span<T> read_asset_data(u8*& ptr, u32 count) {
    auto ret = std::span<T>((T*)ptr, count);
    ptr += count * sizeof(T);
    return ret;
}

void Scene::load_asset_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ERROR("Failed to open asset file at {}", path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    m_asset_file_mem = std::vector<u8>(size); 
    if (!file.read((char*)m_asset_file_mem.data(), size)) {
        ERROR("Failed to read all the contents of the asset file: {}", path);
    }
    
    file.close();

    AssetHeader* header = (AssetHeader*)m_asset_file_mem.data();
    u8* ptr = m_asset_file_mem.data() + sizeof(AssetHeader);

    m_indices = read_asset_data<u32>(ptr, header->num_indices);
    m_vertices = read_asset_data<Vertex>(ptr, header->num_vertices);
    m_meshes = read_asset_data<Mesh>(ptr, header->num_meshes);
    m_primitives = read_asset_data<Primitive>(ptr, header->num_primitives);
    m_nodes = read_asset_data<Node>(ptr, header->num_nodes);
    m_root_nodes = read_asset_data<u32>(ptr, header->num_root_nodes);

    std::span<u32> mesh_name_indices = read_asset_data<u32>(ptr, header->num_meshes);
    std::span<u8> name_bytes = read_asset_data<u8>(ptr, header->num_name_bytes);

    for (size_t i = 0; i < mesh_name_indices.size() - 1; i++) {
        u32 start = mesh_name_indices[i];
        u32 end = mesh_name_indices[i + 1];
        auto name = std::string((char*)name_bytes.data() + start, end - start);
        m_names_to_mesh[name] = MeshHandle(i);
    }
    {
        u32 start = mesh_name_indices[header->num_meshes - 1];
        u32 end = name_bytes.size();
        auto name = std::string((char*)name_bytes.data() + start, end - start);
        m_names_to_mesh[name] = MeshHandle(header->num_meshes - 1);
    }

    u64 bytes_read = (u64)(ptr - m_asset_file_mem.data());
    assert(bytes_read == m_asset_file_mem.size() && "Things in asset file that we did not read?");
}

MeshHandle Scene::mesh_from_name(std::string name) {
    if (m_names_to_mesh.find(name) == m_names_to_mesh.end()) {
        return MeshHandle(UINT32_MAX);
    }

    return m_names_to_mesh[name];
}


void Scene::compute_global_node_transforms() {
    m_global_node_transforms.resize(m_nodes.size());
    for (u32 root_node_index : m_root_nodes) {
        calc_global_node_transform(NodeHandle(root_node_index), glm::mat4(1.0f));
    }
}

void Scene::calc_global_node_transform(NodeHandle node_handle, const glm::mat4& parent_transform) {
    const auto& node = m_nodes[node_handle.get_value()];

    auto T = glm::translate(glm::mat4(1.0f), node.translation);
    auto R = glm::mat4_cast(node.rotation);
    auto S = glm::scale(glm::mat4(1.0f), node.scale);
    auto node_transform = T * R * S;
    auto global_transform = parent_transform * node_transform;
    m_global_node_transforms[node_handle.get_value()] = global_transform;

    for (size_t i = 0; i < node.num_children; i++) {
        calc_global_node_transform(NodeHandle(node.child_index + i), global_transform);
    }
}

}