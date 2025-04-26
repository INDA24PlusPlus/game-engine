#include "Node.h"

#include <cstdint>
#include <cstring>

#include "Scene.h"
#include "AssetManifest.h"

namespace engine {

void NodeHierarchy::to_immutable_internal(AssetManifest& manifest,
                                          std::vector<ImmutableNode>& immutable, u32 node_index,
                                          u32 mut_node_index) {
    u32 child_index = immutable.size();
    const auto& mut_node = m_nodes[mut_node_index];

    immutable[node_index] = {
        .name = manifest.create_name(mut_node.name),
        .rotation = mut_node.rotation,
        .translation = mut_node.translation,
        .child_index = child_index,
        .scale = mut_node.scale,
        .num_children = (u32)mut_node.children.size(),
        .mesh_index = mut_node.kind == Node::Kind::mesh ? mut_node.mesh_index : UINT32_MAX,
    };
    immutable.resize(immutable.size() + mut_node.children.size());

    for (size_t i = 0; i < mut_node.children.size(); ++i) {
        to_immutable_internal(manifest, immutable, child_index + i, mut_node.children[i]);
    }
}

std::vector<ImmutableNode> NodeHierarchy::to_immutable(AssetManifest& manifest, NodeHandle handle) {
    std::vector<ImmutableNode> immutable;
    immutable.resize(1);
    to_immutable_internal(manifest, immutable, 0, handle.get_value());

    return immutable;
}

NodeHandle NodeHierarchy::instantiate_prefab(const Scene& scene, const Prefab& prefab, NodeHandle parent_node) {
    u32 node_index = instantiate_prefab_node(scene, prefab.root_node_index);
    m_nodes[parent_node.get_value()].children.push_back(node_index);

    return NodeHandle(node_index);
}

u32 NodeHierarchy::instantiate_prefab_node(const Scene& scene, u32 node_index) {
    const auto& prefab_node = scene.m_prefab_nodes[node_index];
    bool is_mesh_node = prefab_node.mesh_index != UINT32_MAX;

    u32 our_node_index = m_nodes.size();
    m_nodes.push_back(Node{
        .kind = is_mesh_node ? Node::Kind::mesh : Node::Kind::node,
        .name = scene.m_manifest.get_name_data(prefab_node.name).data(),
        .rotation = prefab_node.rotation,
        .translation = prefab_node.translation,
        .scale = prefab_node.scale,
        .mesh_index = prefab_node.mesh_index,
    });

    for (size_t i = 0; i < prefab_node.num_children; ++i) {
        u32 child_index = instantiate_prefab_node(scene, prefab_node.child_index + i);
        m_nodes[our_node_index].children.push_back(child_index);
    }

    return our_node_index;
}

}  // namespace engine