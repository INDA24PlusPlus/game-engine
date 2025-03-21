
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include "tiny_gltf.h"

#include <print>
#include <string>
#include <fstream>
#include <cstdint>

struct Mesh {
	uint32_t num_indices;
	uint32_t num_vertices;
};

struct Slice {
	char* ptr;
	size_t len;
};

int main(void) {
	std::string path = "../../assets/monkey/monkey.gltf";

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool res = loader.LoadASCIIFromFile(&model, &err, &warn, path);

	if (!warn.empty()) {
		std::println("Warn: {}", warn);
	}
	if (!err.empty()) {
		std::println("Err: {}", err);
	}

	if (!res) {
		std::println("Failed to parse glTF");
		return 1;
	}

	if (model.scenes.size() != 1) {
		std::println("Asset processor only accepts glTF files with one scene");
		return 1;
	}
	if (model.nodes.size() != 1) {
		std::println("Asset processor only accepts glTF files with one node (subject to change)");
		return 1;
	}
	if (model.meshes.size() != 1) {
		std::println("Asset processor only accepts glTF files with one mesh (subject to change)");
		return 1;
	}
	if (model.accessors.size() != 2) {
		std::println("Asset processor only accepts glTF files with two accessors (Vertex position and indices) (subject to change)");
		return 1;
	}

	// FIXME: Directly load the mesh for now. Later we want to traverse the scene structure to get the correct transform.
	tinygltf::Mesh& mesh = model.meshes[0];
	if (mesh.primitives.size() != 1) {
		std::println("Asset processor only accepts glTF files with one primitive (subject to change)");
		return 1;
	}

	std::string output_path = "mesh_data.bin";
	std::ofstream outfile(output_path, std::ios::binary);

	Mesh out_mesh;
	Slice vertex_ptr;
	Slice index_ptr;

	for (size_t i = 0; i < mesh.primitives.size(); ++i) {
		tinygltf::Primitive primitive = mesh.primitives[i];
		{
			tinygltf::Accessor index_accessor = model.accessors[primitive.indices];

			if (index_accessor.componentType != 5123 && index_accessor.componentType != 5125) {
				std::println("Index accessor must either be u32 or u16s");
				return 1;
			}
			// Write the index data.
			const tinygltf::BufferView& buffer_view = model.bufferViews[index_accessor.bufferView];
			const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];


			auto addr = (char*)&buffer.data.at(0);
			index_ptr.ptr = addr + buffer_view.byteOffset;
			index_ptr.len = buffer_view.byteLength;
			out_mesh.num_indices = index_accessor.count;
		}
			

		// Write the vertex data.
		for (auto& attrib : primitive.attributes) {
			tinygltf::Accessor pos_accessor = model.accessors[attrib.second];
			if (attrib.first.compare("POSITION") != 0) {
				std::println("The only attribute allowed is the position");
				return 1;
			}

			const tinygltf::BufferView& buffer_view = model.bufferViews[pos_accessor.bufferView];
			const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

			auto addr = (char*)&buffer.data.at(0);
			vertex_ptr.ptr = addr + buffer_view.byteOffset;
			vertex_ptr.len = buffer_view.byteLength;
			out_mesh.num_vertices = pos_accessor.count;
		}
	}

	outfile.write((char*)&out_mesh, sizeof(Mesh));
	outfile.write(index_ptr.ptr, index_ptr.len);
	outfile.write(vertex_ptr.ptr, vertex_ptr.len);

	outfile.close();
}
