#include "Scene.h"
#include <cstdint>
#include <fstream>

#include "utils/logging.h"

namespace engine {

template <typename T>
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
    INFO("------- Asset Header -------");
    INFO("Num Indices: {}", header->num_indices);
    INFO("Num Vertices: {}", header->num_vertices);
    INFO("Num Meshe: {}", header->num_meshes);
    INFO("Num Primitives: {}", header->num_primitives);
    INFO("Num Nodes: {}", header->num_nodes);
    INFO("Num Root nodes: {}", header->num_root_nodes);
    INFO("Num samplers: {}", header->num_samplers);
    INFO("Num images: {}", header->num_images);
    INFO("Num textures: {}", header->num_textures);
    INFO("Num materials: {}", header->num_materials);
    INFO("Asset file is {} bytes ({} MB)", m_asset_file_mem.size(), m_asset_file_mem.size() >> 20);

    u8* ptr = m_asset_file_mem.data() + sizeof(AssetHeader);

    m_indices = read_asset_data<u8>(ptr, header->num_indices);
    m_vertices = read_asset_data<Vertex>(ptr, header->num_vertices);
    m_meshes = read_asset_data<Mesh>(ptr, header->num_meshes);
    m_primitives = read_asset_data<Primitive>(ptr, header->num_primitives);
    m_nodes = read_asset_data<Node>(ptr, header->num_nodes);
    m_root_nodes = read_asset_data<u32>(ptr, header->num_root_nodes);
    m_samplers = read_asset_data<SamplerInfo>(ptr, header->num_samplers);
    m_images = read_asset_data<ImageInfo>(ptr, header->num_images);
    m_textures = read_asset_data<TextureInfo>(ptr, header->num_textures);
    m_materials = read_asset_data<Material>(ptr, header->num_materials);
    m_image_data = read_asset_data<u8>(ptr, header->num_image_bytes);
    read_mesh_names(ptr);

    u64 bytes_read = (u64)(ptr - m_asset_file_mem.data());
    if (bytes_read != m_asset_file_mem.size()) {
        ERROR("Asset file is {} bytes but we only read {}, Diff is {}. This should never happen.",
              m_asset_file_mem.size(), bytes_read, (int)m_asset_file_mem.size() - (int)bytes_read);
        exit(1);
    }

    m_global_node_transforms.resize(m_nodes.size());
}

MeshHandle Scene::mesh_from_name(std::string name) {
    if (m_names_to_mesh.find(name) == m_names_to_mesh.end()) {
        ERROR("{} is not a valid mesh!", name);
        return MeshHandle(UINT32_MAX);
    }

    return m_names_to_mesh[name];
}

void Scene::compute_global_node_transforms() {
    for (u32 root_node_index : m_root_nodes) {
        calc_global_node_transform(NodeHandle(root_node_index), glm::mat4(1.0f));
    }
}

void Scene::read_mesh_names(u8*& ptr) {
    u32 num_names_read = 0;
    while (num_names_read < m_meshes.size()) {
        u32 name_length = *(u32*)ptr;
        ptr += 4;
        auto name = std::string((char*)ptr, name_length);
        ptr += name_length;
        MeshHandle handle = *(MeshHandle*)ptr;
        m_names_to_mesh[name] = handle;
        ptr += 4;
        ++num_names_read;
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

}  // namespace engine
