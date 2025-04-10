#include <print>
#include <fstream>
#include "AssetImporter.h"
#include "../../../src/engine/utils/logging.h"

template<typename T>
void write_data(std::vector<T>& data, std::ofstream& stream, u32& num_bytes_written) {
    size_t size = sizeof(T) * data.size();
    stream.write((const char*)data.data(), size);
    num_bytes_written += size;
}

int main(int argc, const char** argv) {
    AssetImporter importer;
    if (argc <= 1) {
        std::println("Usage {} <gltf_file> <gltf_file>...", argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        importer.load_asset(argv[i]);
    }

    AssetHeader header;
    header.num_indices = importer.m_indices.size();
    header.num_vertices = importer.m_vertices.size();
    header.num_meshes = importer.m_meshes.size();
    header.num_primitives = importer.m_primitives.size();
    header.num_nodes = importer.m_nodes.size();
    header.num_root_nodes = importer.m_root_nodes.size();

    std::ofstream out_file("scene_data.bin", std::ios::binary);

    // Write header.
    out_file.write((const char*)&header, sizeof(AssetHeader));
    u32 num_bytes_written;
    write_data(importer.m_indices, out_file, num_bytes_written);
    write_data(importer.m_vertices, out_file, num_bytes_written);
    write_data(importer.m_meshes, out_file, num_bytes_written);
    write_data(importer.m_primitives, out_file, num_bytes_written);
    write_data(importer.m_nodes, out_file, num_bytes_written);
    write_data(importer.m_root_nodes, out_file, num_bytes_written);
    INFO("wrote {} bytes", num_bytes_written + sizeof(AssetHeader));

    out_file.flush();
    out_file.close();
}
