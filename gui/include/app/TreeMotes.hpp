#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "app/Particle.hpp"

class Shader;

void drawTreeMotes(std::vector<Spark>& motes, bool charging, float chargeT,
                   bool justReleased, const glm::vec3& treePos,
                   Shader& star, unsigned vao, const glm::mat4& view,
                   const glm::mat4& proj, float elapsed, float dt);
