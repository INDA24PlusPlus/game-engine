#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
struct Player {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    float health;
    float speed;
};