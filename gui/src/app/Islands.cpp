#include "app/Islands.hpp"
#include <cmath>
#include <algorithm>

static constexpr float PI  = 3.14159265f;
static constexpr float GAP = 0.5f;

static float ringRadius(int count, float baseR)
{
    if (count <= 1) return 0.f;
    return (baseR + baseR * GAP * 0.5f) / std::sin(PI / (float)count);
}

glm::vec2 islandCenter(int idx, int count, float cx, float cz, float baseR)
{
    if (count <= 1) return { cx, cz };
    float ring = ringRadius(count, baseR);
    float a = 2.f * PI * (float)idx / (float)count - PI / 2.f;
    return { cx + ring * std::cos(a), cz + ring * std::sin(a) };
}

float islandRadius(int , float baseR)
{

    return baseR;
}

float worldExtentRadius(int count, float baseR)
{
    if (count <= 1) return baseR;
    return ringRadius(count, baseR) + baseR;
}
