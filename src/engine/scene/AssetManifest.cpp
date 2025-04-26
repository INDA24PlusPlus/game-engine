#include "AssetManifest.h"

namespace engine {

void AssetManifest::serialize(std::ofstream& out, u32& bytes_written) {
    // Write the actual name data.
    size_t size = sizeof(u8) * m_name_bytes.size();
    out.write((const char*)m_name_bytes.data(), size);
    bytes_written += size;

    // Mesh names.
    size = sizeof(Name) * m_mesh_names.size();
    out.write((const char*)m_mesh_names.data(), size);
    bytes_written += size;

    // Prefab names.
    size = sizeof(Name) * m_prefab_names.size();
    out.write((const char*)m_prefab_names.data(), size);
    bytes_written += size;
}

void AssetManifest::deserialize(std::span<u8> name_bytes, std::span<Name> mesh_names, std::span<Name> prefab_names) {
    m_name_bytes.assign(name_bytes.begin(), name_bytes.end());
    m_mesh_names.assign(mesh_names.begin(), mesh_names.end());
    m_prefab_names.assign(prefab_names.begin(), prefab_names.end());

    for (size_t i = 0; i < m_prefab_names.size(); ++i) {
        auto name = get_name_data(m_prefab_names[i]);
        m_name_to_prefab[std::string(name)] = i;
    }
}

AssetManifest::Name AssetManifest::create_name(std::string_view name) {
    u32 start = m_name_bytes.size();
    m_name_bytes.resize(m_name_bytes.size() + name.size() + 1);
    std::memcpy(&m_name_bytes[start], name.data(), name.size());
    m_name_bytes[start + name.size()] = 0;

    return { .start = start, .length = (u32)name.size() };
}

}  // namespace engine