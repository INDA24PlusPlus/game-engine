#pragma once

#include <unordered_map>
#include <vector>
#include <fstream>
#include <string_view>
#include <span>

#include "../core.h"


namespace engine {

class AssetManifest {
   public:
    struct Name {
        u32 start;
        u32 length;
    };


    void serialize(std::ofstream& out, u32& bytes_written);
    void deserialize(std::span<u8> name_bytes, std::span<Name> mesh_names, std::span<Name> prefab_names);
    Name create_name(std::string_view name);

    
    inline std::string_view get_mesh_name_data(u32 mesh_index) const {
        auto name = m_mesh_names[mesh_index];
        return std::string_view((char*)&m_name_bytes[name.start], name.length);
    }
    inline std::string_view get_name_data(Name name) const {
        return std::string_view((char*)&m_name_bytes[name.start], name.length);
    }

    std::vector<Name> m_mesh_names;
    std::unordered_map<std::string, u32> m_name_to_mesh;

    std::vector<Name> m_prefab_names;
    std::unordered_map<std::string, u32> m_name_to_prefab;

    std::vector<u8> m_name_bytes;
};

}  // namespace engine