
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include "tiny_gltf.h"

#include <print>
#include <string>
#include <fstream>
#include <cstdint>

struct Mesh {
	uint32_t vertex_start;
	uint32_t vertex_end;
	uint32_t indices_start;
	uint32_t indices_end;
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
			outfile.write(addr + buffer_view.byteOffset, buffer_view.byteLength);
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
			outfile.write(addr + buffer_view.byteOffset,buffer_view.byteLength);
		}
	}

	outfile.close();
}
