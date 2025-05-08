
#include "engine/ecs/component.hpp"
#include "engine/scene/Node.h"
#include "glm/fwd.hpp"
class CVelocity : public Component<CVelocity> {
public:
  glm::vec3 vel;
};

class CTranslation : public Component<CTranslation> {
public:
  glm::vec3 pos;
  glm::quat rot;
  glm::vec3 scale;
};

class CMesh : public Component<CMesh> {
public:
  engine::NodeHandle m_node;
  CMesh(engine::NodeHandle node) : m_node(node) {}
  CMesh() {}
};
