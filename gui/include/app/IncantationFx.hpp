#pragma once
#include <glm/glm.hpp>
#include <vector>

class Shader;

struct IncantationEffect {
    glm::vec3 pos;
    float     timer    = 0.f;
    bool      success  = false;
};
constexpr float INCANT_DUR = 6.f;

void drawIncantations(const std::vector<IncantationEffect>& fx, Shader& incant,
                      const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& camPos, float elapsed);
