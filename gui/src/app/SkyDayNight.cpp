#include "app/SkyDayNight.hpp"
#include <cstdlib>
#include <cmath>

DayNight computeDayNight(float elapsed, int nightOverride)
{
    const float DAY_LENGTH = std::getenv("ZAPPY_DAY_LENGTH")
                           ? (float)std::atof(std::getenv("ZAPPY_DAY_LENGTH")) : 120.f;
    const float DAY_START  = std::getenv("ZAPPY_DAY_START")
                           ? (float)std::atof(std::getenv("ZAPPY_DAY_START")) : 0.05f;
    float dayPhase = std::fmod(elapsed / DAY_LENGTH + DAY_START, 1.f);

    float nightF = glm::clamp(glm::smoothstep(0.45f, 0.55f, dayPhase)
                            - glm::smoothstep(0.95f, 1.00f, dayPhase), 0.f, 1.f);
    if      (nightOverride == 1) nightF = 1.f;
    else if (nightOverride == 2) nightF = 0.f;

    glm::vec3 skySunDir = glm::normalize(glm::vec3(0.62f, 0.28f, 0.62f));
    glm::vec3 moonDir   = glm::normalize(glm::vec3(-0.593f, 0.545f, -0.593f));
    glm::vec3 sunDir    = glm::normalize(glm::mix(skySunDir, moonDir, nightF));
    glm::vec3 lightCol  = glm::mix(glm::vec3(1.2f, 0.85f, 0.50f),
                                   glm::vec3(0.55f, 0.66f, 0.98f), nightF);
    float ambScale = 1.f - 0.28f * nightF;
    glm::vec3 skyTop = glm::mix(glm::vec3(0.03f,0.05f,0.16f), glm::vec3(0.02f,0.04f,0.14f), nightF);
    glm::vec3 skyHor = glm::mix(glm::vec3(0.55f,0.22f,0.04f), glm::vec3(0.05f,0.08f,0.20f), nightF);

    float duskF = glm::clamp(nightF * (1.f - nightF) * 4.f, 0.f, 1.f);
    skyHor = glm::mix(skyHor, glm::vec3(0.98f,0.42f,0.12f), duskF * 0.6f);
    skyTop = glm::mix(skyTop, glm::vec3(0.45f,0.32f,0.42f), duskF * 0.3f);

    return { nightF, ambScale, skySunDir, moonDir, sunDir, lightCol, skyTop, skyHor };
}
