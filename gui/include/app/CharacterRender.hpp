#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "app/Character.hpp"

class Shader;

void drawCharacters(std::vector<Character>& chars, Shader& skin, Shader& aura,
                    const glm::mat4& view, const glm::mat4& proj,
                    const glm::vec3& camPos, const glm::vec3& sunDir,
                    const glm::vec3& lightCol, float ambScale,
                    float elapsed, float dt);
