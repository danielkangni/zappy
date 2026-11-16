#include "app/IncantationFx.hpp"
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "world/World.hpp"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

void drawIncantations(const std::vector<IncantationEffect>& fx, Shader& incant,
                      const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& camPos, float elapsed)
{
    if (fx.empty()) return;
    static Mesh dashedRing = buildDashedRingMesh(8, 0.55f);
    static Mesh vquad      = buildVQuadMesh();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    incant.use();
    incant.setMat4("uView",    glm::value_ptr(view));
    incant.setMat4("uProj",    glm::value_ptr(proj));
    incant.setVec3("uCamPos",  camPos.x, camPos.y, camPos.z);

    const float DOMAIN_R  = TILE * 0.58f;
    const float INNER_R   = DOMAIN_R * 0.84f;
    const int   N_PILLARS = 8;
    const float PI2       = 6.28318f;

    for (auto& ix : fx) {
        float fadeIn  = glm::clamp(ix.timer / 0.25f, 0.f, 1.f);
        float fadeOut = glm::clamp((INCANT_DUR - ix.timer) / 0.4f, 0.f, 1.f);
        float fade    = fadeIn * fadeOut;
        float ringRot = elapsed * 0.20f;

        incant.setBool("uPillarMode", false);
        incant.setVec3("uRingCenter", ix.pos.x, ix.pos.y, ix.pos.z);
        incant.setFloat("uRingInnerR", INNER_R);
        incant.setFloat("uRingOuterR", DOMAIN_R);
        incant.setVec3 ("uColor", 0.25f, 1.0f, 0.90f);
        incant.setFloat("uAlpha", fade * 0.90f);
        {
            glm::mat4 Mr = glm::translate(glm::mat4(1.f), ix.pos + glm::vec3(0, 0.02f, 0));
            Mr = glm::rotate(Mr, ringRot, {0.f, 1.f, 0.f});
            Mr = glm::scale(Mr, glm::vec3(DOMAIN_R));
            glm::mat3 Nr = glm::mat3(glm::transpose(glm::inverse(Mr)));
            incant.setMat4("uModel",     glm::value_ptr(Mr));
            incant.setMat3("uNormalMat", glm::value_ptr(Nr));
            dashedRing.draw();
        }

        incant.setBool("uPillarMode", true);
        incant.setVec3("uColor", 0.30f, 1.0f, 0.92f);
        for (int i = 0; i < N_PILLARS; ++i) {
            float baseAngle = ringRot + PI2 * (i + 0.75f) / N_PILLARS;
            float px = DOMAIN_R * std::cos(baseAngle);
            float pz = DOMAIN_R * std::sin(baseAngle);

            float phase = std::fmod(elapsed * 1.8f + i * (1.0f / N_PILLARS), 1.0f);
            float h = phase < 0.4f ? phase / 0.4f
                    : phase < 0.7f ? 1.0f
                    : (1.0f - phase) / 0.3f;
            h = h * h;
            float pillarH = h * TILE * 0.55f;
            if (pillarH < 0.005f) continue;

            glm::mat4 Mv = glm::translate(glm::mat4(1.f),
                ix.pos + glm::vec3(px, 0.02f, pz));
            Mv = glm::rotate(Mv, baseAngle, {0.f, 1.f, 0.f});
            Mv = glm::scale(Mv, {TILE * 0.055f, pillarH, 1.f});
            glm::mat3 Nv = glm::mat3(glm::transpose(glm::inverse(Mv)));
            incant.setFloat("uAlpha", fade * h * 0.80f);
            incant.setMat4("uModel",     glm::value_ptr(Mv));
            incant.setMat3("uNormalMat", glm::value_ptr(Nv));
            vquad.draw();
        }
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
}
