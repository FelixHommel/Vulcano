#ifndef VULCANO_SRC_ENGINE_UTILITY_VERTEX_HPP
#define VULCANO_SRC_ENGINE_UTILITY_VERTEX_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace vulc
{

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_UTILITY_VERTEX_HPP

