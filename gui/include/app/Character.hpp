#pragma once
#include <glm/glm.hpp>
#include "render/SkinnedMesh.hpp"

enum class CharState { IDLE, FALLING, GROUNDED, RISING };

struct Character {
    SkinnedMesh mesh;
    glm::vec3   color;
    glm::vec3   color2 = color;
    glm::vec3   color3 = color;
    float       split1 = 0.f;
    float       split2 = 0.f;
    glm::vec3   worldPos;
    float       rotY;
    float       scale      = 1.f;
    int         idleAnim   = 0;
    int         fallAnim   = -1;
    CharState   state      = CharState::IDLE;
    float       animTime      = 0.f;
    float       stateTimer    = 0.f;
    float       auraTimer     = 0.f;
    bool        auraTriggered = false;
    bool        incanting     = false;
};
