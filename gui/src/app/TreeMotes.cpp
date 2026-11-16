#include "app/TreeMotes.hpp"
#include "render/Shader.hpp"
#include "world/World.hpp"

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>

void drawTreeMotes(std::vector<Spark>& motes, bool charging, float chargeT,
                   bool justReleased, const glm::vec3& treePos,
                   Shader& star, unsigned vao, const glm::mat4& view,
                   const glm::mat4& proj, float elapsed, float dt)
{
    const float baseR = (16.6667f / 1.381f) * 0.8f;

    if (charging) {
        int n = (int)(chargeT * chargeT * 7.f) + (frnd(0.f, 1.f) < chargeT ? 1 : 0);
        for (int i = 0; i < n; ++i) {
            Spark m;
            float ang = frnd(0.f, 6.2832f);
            float rad = baseR * frnd(0.35f, 1.0f);
            m.pos = treePos + glm::vec3(std::cos(ang) * rad, frnd(0.f, 2.f),
                                        std::sin(ang) * rad);
            glm::vec3 inward = glm::normalize(glm::vec3(
                treePos.x - m.pos.x, 0.f, treePos.z - m.pos.z) + glm::vec3(1e-4f));
            m.vel = glm::vec3(inward.x * frnd(0.4f, 1.3f), frnd(2.5f, 5.5f),
                              inward.z * frnd(0.4f, 1.3f));
            m.maxLife = frnd(1.0f, 2.0f); m.life = m.maxLife;
            m.size  = frnd(0.12f, 0.30f);
            m.phase = frnd(0.f, 6.28f); m.twFreq = frnd(16.f, 36.f);
            m.color = glm::mix(glm::vec3(0.85f, 0.95f, 0.30f),
                               glm::vec3(0.25f, 1.0f, 0.40f), chargeT)
                      * frnd(0.8f, 1.2f);
            motes.push_back(m);
        }
    }
    if (justReleased) {
        const int n = 150;
        for (int i = 0; i < n; ++i) {
            Spark m;
            float ang = (i / (float)n) * 6.2832f + frnd(-0.04f, 0.04f);
            float rad = baseR * frnd(0.2f, 0.6f);
            m.pos = treePos + glm::vec3(std::cos(ang) * rad, frnd(0.3f, 1.6f),
                                        std::sin(ang) * rad);
            float spd = frnd(10.f, 17.f);
            m.vel = glm::vec3(std::cos(ang) * spd, frnd(-0.4f, 1.2f), std::sin(ang) * spd);
            m.maxLife = frnd(1.6f, 2.8f); m.life = m.maxLife;
            m.size  = frnd(0.16f, 0.36f);
            m.phase = frnd(0.f, 6.28f); m.twFreq = frnd(14.f, 32.f);
            m.color = glm::vec3(0.28f, 1.0f, 0.45f) * frnd(0.85f, 1.25f);
            motes.push_back(m);
        }
    }

    for (auto& m : motes) {
        glm::vec2 rel(m.pos.x - treePos.x, m.pos.z - treePos.z);
        float a = 0.9f * dt, c = std::cos(a), s = std::sin(a);
        m.pos.x = treePos.x + (rel.x * c - rel.y * s);
        m.pos.z = treePos.z + (rel.x * s + rel.y * c);
        m.pos += m.vel * dt;
        m.life -= dt;
    }
    motes.erase(std::remove_if(motes.begin(), motes.end(),
        [](const Spark& s) { return s.life <= 0.f; }), motes.end());

    if (motes.empty()) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    star.use();
    star.setMat4("uView", glm::value_ptr(view));
    star.setMat4("uProj", glm::value_ptr(proj));
    glBindVertexArray(vao);
    for (auto& m : motes) {
        float k  = m.life / m.maxLife;
        float tw = 0.5f + 0.5f * std::sin(elapsed * m.twFreq + m.phase);
        star.setVec3 ("uCenter", m.pos.x, m.pos.y, m.pos.z);
        star.setFloat("uSize",   m.size);
        star.setVec3 ("uColor",  m.color.r, m.color.g, m.color.b);
        star.setFloat("uIntensity", (0.30f + 1.0f * k) * tw);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}
