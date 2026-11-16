#include "app/Wildlife.hpp"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdlib>

static float frand(float lo, float hi)
{
    return lo + (hi - lo) * (std::rand() / (float)RAND_MAX);
}

static glm::vec3 randomTarget(const WildlifeEntity &e, float cx, float cz, float radius)
{

    float ang = frand(0.f, 6.2831853f);
    float r   = radius * std::sqrt(frand(0.f, 1.f));
    return { cx + r * std::cos(ang), frand(e.minY, e.maxY), cz + r * std::sin(ang) };
}

void updateWildlife(WildlifeGroup &g, float dt, float cx, float cz, float radius)
{
    for (auto &e : g.entities) {
        e.animTime += dt;

        glm::vec3 diff = e.target - e.pos;
        float dist = glm::length(diff);

        if (dist < 0.3f) {
            e.target = randomTarget(e, cx, cz, radius);
            diff = e.target - e.pos;
            dist = glm::length(diff);
        }

        glm::vec3 dir = diff / dist;
        e.pos += dir * e.speed * dt;

        float targetYaw = std::atan2(dir.x, dir.z) * (180.f / 3.14159265f);
        float delta = targetYaw - e.rotY;

        while (delta >  180.f) delta -= 360.f;
        while (delta < -180.f) delta += 360.f;
        e.rotY += delta * std::min(1.f, dt * 4.f);
    }
}

void drawWildlife(WildlifeGroup &g, Shader &skin,
                  const glm::mat4 &view, const glm::mat4 &proj,
                  const glm::vec3 &camPos,
                  const glm::vec3 &sunDir, const glm::vec3 &lightCol,
                  float ambScale, float elapsed)
{
    if (g.entities.empty() || g.mesh.boneCount() == 0) return;

    glm::mat4 boneMats[MAX_BONES];

    glDisable(GL_CULL_FACE);

    skin.use();
    skin.setMat4("uView",            glm::value_ptr(view));
    skin.setMat4("uProj",            glm::value_ptr(proj));
    skin.setVec3("uCamPos",          camPos.x,    camPos.y,    camPos.z);
    skin.setVec3("uDirLightDir",     sunDir.x,    sunDir.y,    sunDir.z);
    skin.setVec3("uDirLightColor",   lightCol.x,  lightCol.y,  lightCol.z);
    skin.setVec3("uPointLight0Color",0.f, 0.f, 0.f);
    skin.setVec3("uPointLight1Color",0.f, 0.f, 0.f);
    skin.setFloat("uShininess",      16.f);
    skin.setFloat("uAmbientStrength",0.30f * ambScale);
    skin.setFloat("uAlpha",          1.0f);
    skin.setFloat("uTime",           elapsed);
    skin.setBool ("uEmissive",       false);
    skin.setBool ("uCrystal",        false);
    skin.setBool ("uWave",           false);
    skin.setBool ("uSway",           false);
    skin.setBool ("uCircleClip",     false);
    skin.setBool ("uShoreBlend",     false);
    skin.setFloat("uShoreInnerR",    0.f);
    skin.setFloat("uShoreWidth",     1.f);
    skin.setBool ("uUseTexture",     true);
    skin.setBool ("uAlphaCutout",    g.alphaCutout);
    skin.setInt  ("uTexture",        0);
    skin.setBool ("uUseVertexColor", false);
    skin.setBool ("uMultiColor",     false);
    skin.setVec3 ("uColor2",         1.f, 1.f, 1.f);
    skin.setVec3 ("uColor3",         1.f, 1.f, 1.f);
    skin.setFloat("uSplit1",         0.f);
    skin.setFloat("uSplit2",         0.f);

    for (auto &e : g.entities) {
        g.mesh.computeBones(0, e.animTime, boneMats, true, false);
        skin.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(boneMats[0]));

        glm::mat4 M = glm::translate(glm::mat4(1.f), e.pos);
        M = glm::rotate(M, glm::radians(e.rotY), {0.f, 1.f, 0.f});
        if (g.baseRoll != 0.f) M = glm::rotate(M, glm::radians(g.baseRoll), {0.f, 0.f, 1.f});
        if (g.baseTilt != 0.f) M = glm::rotate(M, glm::radians(g.baseTilt), {1.f, 0.f, 0.f});
        M = glm::scale(M, glm::vec3(g.scale));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skin.setMat4("uModel",     glm::value_ptr(M));
        skin.setMat3("uNormalMat", glm::value_ptr(N3));
        g.mesh.draw();
    }

    glEnable(GL_CULL_FACE);
}
