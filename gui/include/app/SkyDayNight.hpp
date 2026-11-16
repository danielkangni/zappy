#pragma once
#include <glm/glm.hpp>

struct DayNight {
    float     nightF;
    float     ambScale;
    glm::vec3 skySunDir;
    glm::vec3 moonDir;
    glm::vec3 sunDir;
    glm::vec3 lightCol;
    glm::vec3 skyTop;
    glm::vec3 skyHor;
};

DayNight computeDayNight(float elapsed, int nightOverride);
