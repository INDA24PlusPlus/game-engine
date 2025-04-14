#include <fstream>
#include <ostream>
#include <print>

#include "../../../src/engine/utils/logging.h"
#include "AssetImporter.h"


template <typename T>
void write_data(std::vector<T>& data, std::ofstream& stream, u32& num_bytes_written) {
    size_t size = sizeof(T) * data.size();
    stream.write((const char*)data.data(), size);
    num_bytes_written += size;
}

static void write_metadata_file(const AssetImporter& importer) {
    std::ofstream file("metadata.txt");

    for (auto it : importer.m_mesh_names) {
        file << it.first << ": " << it.second << '\n';
    }

    file.flush();
    file.close();
}

static void write_mesh_names(const AssetImporter& importer, std::ofstream& out_file,
                             u32& num_bytes_written) {
    for (auto it : importer.m_mesh_names) {
        u32 size = it.first.size();
        out_file.write((char*)&size, sizeof(u32));
        out_file.write(it.first.c_str(), size);
        out_file.write((char*)&it.second, sizeof(u32));
        num_bytes_written += size + 2 * sizeof(u32);
    }
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
    header.num_samplers = importer.m_samplers.size();
    header.num_images = importer.m_images.size();
    header.num_textures = importer.m_textures.size();
    header.num_materials = importer.m_materials.size();
    header.num_image_bytes = importer.m_image_data.size();

    std::ofstream out_file("scene_data.bin", std::ios::binary);

    // Write header.
    out_file.write((const char*)&header, sizeof(AssetHeader));
    u32 num_bytes_written = 0;
    write_data(importer.m_indices, out_file, num_bytes_written);
    write_data(importer.m_vertices, out_file, num_bytes_written);
    write_data(importer.m_meshes, out_file, num_bytes_written);
    write_data(importer.m_primitives, out_file, num_bytes_written);
    write_data(importer.m_nodes, out_file, num_bytes_written);
    write_data(importer.m_root_nodes, out_file, num_bytes_written);
    write_data(importer.m_samplers, out_file, num_bytes_written);
    write_data(importer.m_images, out_file, num_bytes_written);
    write_data(importer.m_textures, out_file, num_bytes_written);
    write_data(importer.m_materials, out_file, num_bytes_written);
    write_data(importer.m_image_data, out_file, num_bytes_written);
    write_mesh_names(importer, out_file, num_bytes_written);
    INFO("wrote {} bytes", num_bytes_written + sizeof(AssetHeader));

    out_file.flush();
    out_file.close();

    write_metadata_file(importer);
}
