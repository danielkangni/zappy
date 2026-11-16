#pragma once
#include <glm/glm.hpp>

glm::vec2 islandCenter(int idx, int count, float cx, float cz, float baseR);

float     islandRadius(int count, float baseR);

float     worldExtentRadius(int count, float baseR);
