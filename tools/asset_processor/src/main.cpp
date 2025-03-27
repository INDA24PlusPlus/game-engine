#include <cassert>
#include <fstream>
#include <limits>
#include <print>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <utility>
#include <vector>

#include "../../../src/engine/assets.h"
#include "assimp/matrix4x4.h"
#include "assimp/mesh.h"
#include "glm/fwd.hpp"

struct Context {
    engine::asset::Header header;
    std::vector<engine::asset::StaticMesh> static_meshes;
    std::vector<engine::asset::SkinnedMesh> skinned_meshes;
    std::vector<engine::asset::Vertex> static_vertices;
    std::vector<engine::asset::SkinnedVertex> skinned_vertices;
    std::vector<engine::asset::IndexType> indices;
    std::vector<engine::asset::Node> nodes;
};

void parse_static_mesh_data(Context& context, const aiMesh* mesh) {
    for (size_t i = 0; i < mesh->mNumVertices; ++i) {
        auto pos = &mesh->mVertices[i];
        f32 uv[2] = {};

        if (mesh->mTextureCoords[0] != nullptr) {
            uv[0] = mesh->mTextureCoords[0][i].x;
            uv[1] = mesh->mTextureCoords[0][i].y;
        }

        context.static_vertices.push_back(engine::asset::Vertex{
            .pos = { pos->x, pos->y, pos->z },
            .normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z },
            .uv = { uv[0], uv[1] },
        });

    } 

    for (size_t i = 0; i < mesh->mNumFaces; ++i) {
        auto face = &mesh->mFaces[i];
        context.indices.push_back((engine::asset::IndexType)face->mIndices[0]);
        context.indices.push_back((engine::asset::IndexType)face->mIndices[1]);
        context.indices.push_back((engine::asset::IndexType)face->mIndices[2]);
    } 
}

void add_skinning_data(Context& context, u32 vertex_idx, u32 bone_idx, f32 weight) {
    engine::asset::SkinnedVertex& vertex = context.skinned_vertices[vertex_idx];
    for (size_t i = 0; i < engine::asset::bones_per_vertex; ++i) {
        if (vertex.bone_weights[i] == 0) {
            vertex.bone_weights[i] = (u8)(weight * 255.0f);
            vertex.bone_indices[i] = bone_idx;
            return;
        }
    }

    std::println("Vertex in mesh is connected to more than the maximum of 4 bones.");
    exit(1);
}

void parse_mesh_bone(Context& context, const engine::asset::SkinnedMesh& mesh, u32 bone_idx, const aiBone* bone) {
    for (size_t i = 0; i < bone->mNumWeights; ++i) {
        const aiVertexWeight& weight = bone->mWeights[i];
        u32 vertex_idx = weight.mVertexId + mesh.base_vertex;
        add_skinning_data(context, vertex_idx, bone_idx, weight.mWeight);
    }
}

void parse_skinned_mesh_data(Context& context, const engine::asset::SkinnedMesh& our_mesh, const aiMesh* mesh) {
    for (size_t i = 0; i < mesh->mNumVertices; ++i) {
        auto pos = &mesh->mVertices[i];
        context.skinned_vertices.push_back({
            .pos = { pos->x, pos->y, pos->z },
            .bone_indices = {255, 255, 255, 255},
            //.bone_weights = {0, 0, 0, 0}, // make sure all weighst are zero.
        });
    } 

    for (size_t i = 0; i < mesh->mNumFaces; ++i) {
        auto face = &mesh->mFaces[i];
        context.indices.push_back((engine::asset::IndexType)face->mIndices[0]);
        context.indices.push_back((engine::asset::IndexType)face->mIndices[1]);
        context.indices.push_back((engine::asset::IndexType)face->mIndices[2]);
    }


    for (size_t i = 0; i < mesh->mNumBones; ++i) {
        parse_mesh_bone(context, our_mesh, i, mesh->mBones[i]);
    }
}

void parse_meshes(Context& context, const aiScene* scene) {
    context.header = {};

    for (size_t i = 0; i < scene->mNumMeshes; ++i) {
        auto mesh = scene->mMeshes[i];
        if ((mesh->mNumFaces * 3) >= (std::numeric_limits<engine::asset::IndexType>::max())) {
            std::println("Fatal error: Mesh '{}' has more than 65536 indices, contact Vidar if you want this restriction lifted.", mesh->mName.C_Str());
            exit(1);
        }

        if ((mesh->mNumBones) >= (1 << 8)) {
            std::println("Fatal error: Mesh '{}' has more than 256 bones, contact Vidar if you want this restriction lifted.", mesh->mName.C_Str());
            exit(1);
        }

        if (mesh->HasBones()) {
            context.skinned_meshes.push_back({
                .base_vertex = context.header.num_skinned_vertices,
                .num_vertices = mesh->mNumVertices,
                .base_index = context.header.num_indices,
                .num_indices = mesh->mNumFaces * 3,
                .base_bone = context.header.num_bones,
                .num_bones = mesh->mNumBones,
          });
          context.header.num_skinned_vertices += mesh->mNumVertices;
          context.header.num_bones += mesh->mNumBones;
        } else {
            context.static_meshes.push_back({
                .base_vertex = context.header.num_static_vertices,
                .num_vertices = mesh->mNumVertices,
                .base_index = context.header.num_indices,
                .num_indices = mesh->mNumFaces * 3,
            });
            context.header.num_static_vertices += mesh->mNumVertices;
        }

        context.header.num_indices += mesh->mNumFaces * 3;
    }
    context.header.num_static_meshes = context.static_meshes.size();
    context.header.num_skinned_meshes = context.skinned_meshes.size();

    // Sanity check.
    if (context.header.num_indices % 3 != 0) {
        std::println("Fatal error: The number of indices was not a multiple of three (non triangle faces?)");
        exit(1);
    }

    context.static_vertices.reserve(context.header.num_static_vertices);
    context.skinned_vertices.reserve(context.header.num_skinned_vertices);
    context.indices.reserve(context.header.num_indices);

    for (size_t i = 0; i < scene->mNumMeshes; ++i) {
        if (scene->mMeshes[i]->HasBones()) {
            parse_skinned_mesh_data(context, context.skinned_meshes[i], scene->mMeshes[i]);
        } else {
            parse_static_mesh_data(context, scene->mMeshes[i]);
        }
    }
}

glm::mat4 to_glm_matrix(const aiMatrix4x4 &from) {
  return glm::mat4(from.a1, from.b1, from.c1, from.d1, from.a2, from.b2,
                   from.c2, from.d2, from.a3, from.b3, from.c3, from.d3,
                   from.a4, from.b4, from.c4, from.d4);
}

void add_bone_payload(engine::asset::Node& our_node, const aiNode* node) {
    our_node.kind = engine::asset::Node::Kind::bone;
    std::unreachable();
}

void add_mesh_payload(engine::asset::Node& our_node, const aiNode* node) {
    our_node.kind = engine::asset::Node::Kind::mesh;
    our_node.mesh_data.num_meshes = node->mNumMeshes;
    for (size_t i = 0; i < node->mNumMeshes; i++) {
        our_node.mesh_data.mesh_indices[i] = node->mMeshes[i];
    }
}

void parse_node(Context& context, const aiScene* scene, const aiNode* node, u32 node_idx) {
    if (node->mNumMeshes > 4) {
        std::println("Fatal error: Node has more than four meshes. Contact Vidar if you want this restriction chagned");
        exit(1);
    }

    std::println("parsing node: {}, {} children, {} meshes", node->mName.C_Str(), node->mNumChildren, node->mNumMeshes);

    bool is_mesh_node = node->mNumMeshes;
    bool is_bone_node = false; // TODO: Fix this
    assert(!(is_bone_node && is_mesh_node) && "node has both mesh and bone?");


    u32 children_index = (u32)context.nodes.size();
    context.nodes[node_idx] = engine::asset::Node {
        .transform = to_glm_matrix(node->mTransformation),
        .child_count = node->mNumChildren,
        .child_index = children_index,
        .kind = engine::asset::Node::Kind::none,
    };

    if (is_mesh_node) {
        add_mesh_payload(context.nodes[node_idx], node);
    }
    if (is_bone_node) {
        add_bone_payload(context.nodes[node_idx], node);
    }

    context.nodes.resize(context.nodes.size() + node->mNumChildren);
    for (size_t i = 0; i < node->mNumChildren; ++i) {
        parse_node(context, scene, node->mChildren[i], children_index + i);
    }
}

void parse_scene(Context& context, const aiScene* scene) {
    parse_meshes(context, scene);

    context.nodes.resize(1);
    parse_node(context, scene, scene->mRootNode, 0);
    context.header.num_nodes = context.nodes.size();
}

// NOTE: Files generated by the asset processor will only work on the native endian that the file was generated on.
int main(int argc, char* argv[]) {
    std::println("Here\n");
    if (argc != 2) {
        std::println("Usage: {} <model file path>", argv[0]);
        return 1;
    }

    char* file_path = argv[1];
    if (sizeof(aiMatrix4x4t<float>) != sizeof(f32[16])) {
        std::println("Fix this.");
        exit(1);
    }

    auto process_flags =  aiProcess_Triangulate
                        | aiProcess_GenNormals
                        | aiProcess_JoinIdenticalVertices
                        | aiProcess_CalcTangentSpace;

    Assimp::Importer importer;
    auto scene = importer.ReadFile(file_path, process_flags);
    if (!scene) {
        std::println("Error parsing '{}': {}", file_path, importer.GetErrorString());
        return 1;
    }

    Context context;
    parse_scene(context, scene);
    std::println("we have {} nodes", context.header.num_nodes);
    std::ofstream out_file("mesh_data_new.bin", std::ios::binary);

    // Write header.
    out_file.write((const char*)&context.header, sizeof(engine::asset::Header));

    // Write static meshes
    out_file.write((const char*)context.static_meshes.data(), sizeof(engine::asset::StaticMesh) * context.static_meshes.size());

    // Write skinned meshes
    out_file.write((const char*)context.skinned_meshes.data(), sizeof(engine::asset::SkinnedMesh) * context.skinned_meshes.size());

    // Write nodes
    out_file.write((const char*)context.nodes.data(), sizeof(engine::asset::Node) * context.nodes.size());

    // Write vertices
    out_file.write((const char*)context.static_vertices.data(), sizeof(engine::asset::Vertex) * context.static_vertices.size());

    // Write skinned vertices
    out_file.write((const char*)context.skinned_vertices.data(), sizeof(engine::asset::SkinnedVertex) * context.skinned_vertices.size());

    // Write indices
    out_file.write((const char*)context.indices.data(), sizeof(engine::asset::IndexType) * context.indices.size());

    out_file.close();
}