#include <cassert>
#include <fstream>
#include <limits>
#include <print>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/gtc/quaternion.hpp>

#include "../../../src/engine/assets.h"
#include "assimp/anim.h"
#include "assimp/matrix4x4.h"
#include "assimp/mesh.h"

struct Context {
    // CPU data
    engine::asset::Header header;
    std::vector<engine::asset::StaticMesh> static_meshes;
    std::vector<engine::asset::SkinnedMesh> skinned_meshes;
    std::vector<engine::asset::Node> nodes;
    std::vector<engine::asset::BoneData> bone_data;
    std::vector<engine::asset::Animation> animations;
    std::vector<engine::asset::NodeAnim> channels;

    std::vector<engine::asset::TranslationKey> translations;
    std::vector<engine::asset::ScaleKey> scalings;
    std::vector<engine::asset::RotationKey> rotations;
    std::vector<glm::mat4> global_inverse_matrices;

    // GPU data
    std::vector<engine::asset::Vertex> static_vertices;
    std::vector<engine::asset::SkinnedVertex> skinned_vertices;
    std::vector<engine::asset::IndexType> indices;

    std::unordered_map<std::string, u32> bone_map;
};

glm::mat4 to_glm_matrix(const aiMatrix4x4 &from) {
    return glm::mat4(from.a1, from.b1, from.c1, from.d1, from.a2, from.b2,
                   from.c2, from.d2, from.a3, from.b3, from.c3, from.d3,
                   from.a4, from.b4, from.c4, from.d4);
}

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

u32 get_bone_index(Context& context, const aiBone* bone) {
    u32 bone_idx = 0;
    std::string bone_name(bone->mName.C_Str());
    if (bone_name.size() == 0) {
        std::println("Fatal error: Bone with no name, (not sure this works)");
        exit(1);
    }

    if (context.bone_map.find(bone_name) == context.bone_map.end()){
        // Create index.
        bone_idx = context.bone_map.size();
        context.bone_map[bone_name] = bone_idx;
        context.bone_data.push_back({ .offset = to_glm_matrix(bone->mOffsetMatrix) });

    } else {
        bone_idx = context.bone_map[bone_name];
    }

    return bone_idx;
}

void add_vertex_skinning_data(Context& context, u32 vertex_idx, u32 bone_idx, f32 weight) {
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
        add_vertex_skinning_data(context, vertex_idx, bone_idx, weight.mWeight);
    }
}

void parse_skinned_mesh_data(Context& context, const engine::asset::SkinnedMesh& our_mesh, const aiMesh* mesh) {
    for (size_t i = 0; i < mesh->mNumVertices; ++i) {
        auto pos = &mesh->mVertices[i];
        context.skinned_vertices.push_back({
            .pos = { pos->x, pos->y, pos->z },
            .bone_indices = {255, 255, 255, 255},
        });
    } 

    for (size_t i = 0; i < mesh->mNumFaces; ++i) {
        auto face = &mesh->mFaces[i];
        context.indices.push_back((engine::asset::IndexType)face->mIndices[0]);
        context.indices.push_back((engine::asset::IndexType)face->mIndices[1]);
        context.indices.push_back((engine::asset::IndexType)face->mIndices[2]);
    }


    for (size_t i = 0; i < mesh->mNumBones; ++i) {
        auto bone = mesh->mBones[i];
        parse_mesh_bone(context, our_mesh, get_bone_index(context, bone), bone);
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
    context.bone_data.reserve(context.header.num_bones);

    for (size_t i = 0; i < scene->mNumMeshes; ++i) {
        if (scene->mMeshes[i]->HasBones()) {
            parse_skinned_mesh_data(context, context.skinned_meshes[i], scene->mMeshes[i]);
        } else {
            parse_static_mesh_data(context, scene->mMeshes[i]);
        }
    }
}

void add_mesh_payload(engine::asset::Node& our_node, const aiNode* node) {
    our_node.kind = engine::asset::Node::Kind::mesh;
    our_node.mesh_data.num_meshes = node->mNumMeshes;
    for (size_t i = 0; i < node->mNumMeshes; i++) {
        our_node.mesh_data.mesh_indices[i] = node->mMeshes[i];
    }
}

void add_bone_payload(Context& context, engine::asset::Node& our_node, const aiNode* node) {
    our_node.kind = engine::asset::Node::Kind::bone;
    our_node.bone_index = context.bone_map[node->mName.C_Str()];
}

void parse_node(Context& context, const aiScene* scene, const aiNode* node, u32 node_idx) {
    if (node->mNumMeshes > 4) {
        std::println("Fatal error: Node has more than four meshes. Contact Vidar if you want this restriction chagned");
        exit(1);
    }

    bool is_mesh_node = node->mNumMeshes;
    bool is_bone_node = context.bone_map.find(node->mName.C_Str()) != context.bone_map.end(); // TODO: Fix this
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
        add_bone_payload(context, context.nodes[node_idx], node);
    }

    context.nodes.resize(context.nodes.size() + node->mNumChildren);
    for (size_t i = 0; i < node->mNumChildren; ++i) {
        parse_node(context, scene, node->mChildren[i], children_index + i);
    }
}

void parse_node_anim(Context& context, const aiNodeAnim* channel, u32 anim_index) {
    context.channels[anim_index] = {
        .is_identity = 0,
        .translation_index = (u32)context.translations.size(),
        .num_translations = channel->mNumPositionKeys,
        .scaling_index = (u32)context.scalings.size(),
        .num_scalings = channel->mNumScalingKeys,
        .rotation_index = (u32)context.rotations.size(),
        .num_rotations = channel->mNumRotationKeys,
    };

    context.header.num_translations += channel->mNumPositionKeys;
    context.header.num_scalings += channel->mNumScalingKeys;
    context.header.num_rotations += channel->mNumRotationKeys;

    for (size_t i = 0; i < channel->mNumRotationKeys; ++i) {
        auto key = channel->mPositionKeys[i];
        if (key.mInterpolation != aiAnimInterpolation_Linear) {
            std::println("Fatal error: Only linear animation interpolation is supported for now.");
            exit(1);
        }

        context.translations.push_back({
            .translation = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z),
            .time = (f32)key.mTime,
        });
    }

    for (size_t i = 0; i < channel->mNumScalingKeys; ++i) {
        auto key = channel->mScalingKeys[i];
        if (key.mInterpolation != aiAnimInterpolation_Linear) {
            std::println("Fatal error: Only linear animation interpolation is supported for now.");
            exit(1);
        }

        context.scalings.push_back({
            .scale = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z),
            .time = (f32)key.mTime,
        });
    }

    for (size_t i = 0; i < channel->mNumRotationKeys; ++i) {
        auto key = channel->mRotationKeys[i];
        if (key.mInterpolation != aiAnimInterpolation_Linear) {
            std::println("Fatal error: Only linear animation interpolation is supported for now.");
            exit(1);
        }

        context.rotations.push_back({
            .rotation = glm::quat(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w),
            .time = (f32)key.mTime,
        });
    }
}

void parse_animations(Context& context, const aiScene* scene) {
    if (!scene->HasAnimations()) return;
    context.header.num_animations = scene->mNumAnimations;

    context.animations.reserve(scene->mNumAnimations);
    context.channels.resize(context.header.num_bones * scene->mNumAnimations);
    for (size_t i = 0; i < context.channels.size(); ++i) {
        context.channels[i] = {
            .is_identity = 1,
        };
    }

    for (size_t i = 0; i < scene->mNumAnimations; ++i) {
        auto anim = scene->mAnimations[i];
        context.animations.push_back({
            .duration = (f32)anim->mDuration,
            .ticks_per_second =  (f32)anim->mTicksPerSecond,
            .channel_index = (u32)context.channels.size(),
            .channel_count = anim->mNumChannels,
        });

        for (size_t j = 0; j < anim->mNumChannels; ++j) {
            auto channel = anim->mChannels[j];
            auto bone_idx = context.bone_map[channel->mNodeName.C_Str()]; // Bone index corresponds to channel index.
            std::println("Parsing animation channel for node {}", channel->mNodeName.C_Str());
            parse_node_anim(context, channel, bone_idx);
        }
    }
}

void parse_scene(Context& context, const aiScene* scene) {
    parse_meshes(context, scene);

    context.nodes.resize(1);
    parse_node(context, scene, scene->mRootNode, 0);
    context.header.num_nodes = context.nodes.size();
    context.header.num_global_inverse_matrices = 1;
    context.global_inverse_matrices.push_back(to_glm_matrix(scene->mRootNode->mTransformation.Inverse()));

    parse_animations(context, scene);
    context.header.num_channels = context.channels.size();
}

template<typename T>
void write_data(std::vector<T>& data, std::ofstream& stream, u32& num_bytes_written) {
    size_t size = sizeof(T) * data.size();
    stream.write((const char*)data.data(), size);
    num_bytes_written += size;
}

// NOTE: Files generated by the asset processor will only work on the native endian that the file was generated on.
int main(int argc, char* argv[]) {
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
    std::ofstream out_file("mesh_data_new.bin", std::ios::binary);

    // Write header.
    out_file.write((const char*)&context.header, sizeof(engine::asset::Header));

    u32 num_bytes_written = 0;
    write_data(context.static_meshes, out_file, num_bytes_written);
    write_data(context.skinned_meshes, out_file, num_bytes_written);
    write_data(context.nodes, out_file, num_bytes_written);
    write_data(context.bone_data, out_file, num_bytes_written);
    write_data(context.animations, out_file, num_bytes_written);
    write_data(context.channels, out_file, num_bytes_written);
    write_data(context.translations, out_file, num_bytes_written);
    write_data(context.scalings, out_file, num_bytes_written);
    write_data(context.rotations, out_file, num_bytes_written);
    write_data(context.global_inverse_matrices, out_file, num_bytes_written);
    context.header.cpu_asset_size = num_bytes_written;
    num_bytes_written = 0;

    // GPU Data
    write_data(context.static_vertices, out_file, num_bytes_written);
    write_data(context.skinned_vertices, out_file, num_bytes_written);
    write_data(context.indices, out_file, num_bytes_written);
    context.header.gpu_asset_size = num_bytes_written;

    // Rewrite header with correct asset sizes.
    out_file.seekp(0);
    out_file.write((const char*)&context.header, sizeof(engine::asset::Header));

    out_file.close();
}