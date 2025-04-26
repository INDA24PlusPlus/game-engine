#include "Scene.h"

#include "engine/AssetLoader.h"
#include "engine/graphics/Sampler.h"
#include "engine/graphics/Image.h"

namespace engine {

void Scene::init(loader::AssetFileData& data) {
    m_manifest.deserialize(data.name_bytes, data.mesh_names, data.prefab_names);

    m_meshes.assign(data.meshes.begin(), data.meshes.end());
    m_primitives.assign(data.primitives.begin(), data.primitives.end());
    m_materials.assign(data.materials.begin(), data.materials.end());
    m_textures.assign(data.textures.begin(), data.textures.end());
    m_prefab_nodes.assign(data.prefab_nodes.begin(), data.prefab_nodes.end());

    for (size_t i = 0; i < data.root_prefab_nodes.size(); ++i) {
        m_prefabs.push_back({
            .name = m_manifest.m_prefab_names[i],
            .root_node_index = data.root_prefab_nodes[i],
        });
    }

    for (const auto& sampler_info : data.samplers) {
        Sampler sampler;
        sampler.init(sampler_info, 1.0f);
        m_samplers.push_back(sampler);
    }

    for (const auto& image_info : data.images) {
        Image image;
        image.init(image_info);
        image.upload(&data.image_data[image_info.image_data_index]);
        m_images.push_back(image);
    }
}

}