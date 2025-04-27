#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

#include "../core.h"
#include "Scene.h"
#include "AssetManifest.h"

namespace engine {

struct NodeTag;
using NodeHandle = TypedHandle<NodeTag>;

// I caved. Fine, lets have a dynamic node hierarchy instead of a immutable one. This means that we
// allow children that are non contigious in memory
struct Node {
    enum class Kind : u32 {
        node = 0,
        mesh = 1,
    };

    // We are already doing dynamic memory allocation per node either way
    // so just keep a copy of the name.
    Kind kind;
    std::string name;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 scale;
    std::vector<u32> children;
    u32 mesh_index;
};

class NodeHierarchy {
   public:
    void init();
    NodeHandle add_root_node(const Node& node);
    NodeHandle add_node(const Node& node, NodeHandle parent);

    std::vector<Node> m_nodes;
    std::vector<ImmutableNode> to_immutable(AssetManifest& manifest, NodeHandle handle);
    void to_immutable_internal(AssetManifest& manifest, std::vector<ImmutableNode>& immutable, u32 node_index,
                               u32 mut_node_index);

    NodeHandle instantiate_prefab(const Scene& scene, const Prefab& prefab, NodeHandle parent_node);
    u32 instantiate_prefab_node(const Scene& scene, u32 node_index);
};

};  // namespace engine