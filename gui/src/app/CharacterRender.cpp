#include "app/CharacterRender.hpp"
#include "render/Shader.hpp"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

void drawCharacters(std::vector<Character>& chars, Shader& skin, Shader& aura,
                    const glm::mat4& view, const glm::mat4& proj,
                    const glm::vec3& camPos, const glm::vec3& sunDir,
                    const glm::vec3& lightCol, float ambScale,
                    float elapsed, float dt)
{
    if (chars.empty()) return;
    glm::mat4 boneMats[MAX_BONES];
    skin.use();
    skin.setMat4("uView", glm::value_ptr(view));
    skin.setMat4("uProj", glm::value_ptr(proj));
    skin.setVec3("uCamPos",          camPos.x, camPos.y, camPos.z);
    skin.setVec3("uDirLightDir",     sunDir.x, sunDir.y, sunDir.z);
    skin.setVec3("uDirLightColor",   lightCol.x, lightCol.y, lightCol.z);
    skin.setVec3("uPointLight0Color",0.f,0.f,0.f);
    skin.setVec3("uPointLight1Color",0.f,0.f,0.f);
    skin.setFloat("uShininess",      32.f);
    skin.setFloat("uAmbientStrength",0.22f * ambScale);
    skin.setFloat("uAlpha",          1.0f);
    skin.setFloat("uTime",           elapsed);
    skin.setBool ("uEmissive",       false);
    skin.setBool ("uCrystal",        false);
    skin.setBool ("uWave",           false);
    skin.setBool ("uSway",           false);
    skin.setBool ("uCircleClip",      false);
    skin.setBool ("uShoreBlend",      false);
    skin.setFloat("uShoreInnerR",     0.f);
    skin.setFloat("uShoreWidth",      1.f);
    skin.setBool ("uUseTexture",      true);
    skin.setBool ("uAlphaCutout",     false);
    skin.setInt  ("uTexture",         0);
    skin.setBool ("uUseVertexColor",  false);
    skin.setBool ("uMultiColor",      false);

    for (auto& ch : chars) {
        if (!ch.incanting) ch.animTime += dt;
        ch.stateTimer += dt;

        int    drawAnim = ch.idleAnim;
        bool   loop     = true;
        bool   rev      = false;

        if (ch.fallAnim >= 0) {
            float fallDur = ch.mesh.animDuration(ch.fallAnim);

            switch (ch.state) {
            case CharState::IDLE:
                drawAnim = ch.idleAnim; loop = true; rev = false;
                if (ch.stateTimer >= 10.f) {
                    ch.state = CharState::FALLING;
                    ch.animTime = 0.f;
                }
                break;

            case CharState::FALLING:
                drawAnim = ch.fallAnim; loop = false; rev = false;
                if (ch.animTime >= fallDur) {
                    ch.state = CharState::GROUNDED;
                    ch.stateTimer = 0.f;
                }
                break;

            case CharState::GROUNDED:
                drawAnim = ch.fallAnim; loop = false; rev = false;
                ch.animTime = fallDur;
                if (ch.stateTimer >= 30.f) {
                    ch.state = CharState::RISING;
                    ch.animTime = 0.f;
                }
                break;

            case CharState::RISING:
                drawAnim = ch.fallAnim; loop = false; rev = true;
                if (ch.animTime >= fallDur) {
                    ch.state = CharState::IDLE;
                    ch.animTime = 0.f;
                    ch.stateTimer = 0.f;
                }
                break;
            }
        }

        ch.mesh.computeBones(drawAnim, ch.animTime, boneMats, loop, rev);
        skin.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(boneMats[0]));

        glm::mat4 M = glm::translate(glm::mat4(1.f), ch.worldPos);
        M = glm::rotate(M, glm::radians(ch.rotY), {0.f,1.f,0.f});
        M = glm::scale(M, glm::vec3(ch.scale));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skin.setMat4("uModel",      glm::value_ptr(M));
        skin.setMat3("uNormalMat",  glm::value_ptr(N3));
        skin.setVec3("uColor2",     ch.color2.r, ch.color2.g, ch.color2.b);
        skin.setVec3("uColor3",     ch.color3.r, ch.color3.g, ch.color3.b);
        skin.setFloat("uSplit1",    ch.split1);
        skin.setFloat("uSplit2",    ch.split2);
        skin.setBool ("uMultiColor",ch.split1 > 0.f);
        ch.mesh.draw();
    }

    bool anyAura = false;
    for (auto& ch : chars) anyAura |= (ch.auraTimer > 0.f);
    if (!anyAura) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    aura.use();
    aura.setMat4("uView", glm::value_ptr(view));
    aura.setMat4("uProj", glm::value_ptr(proj));
    aura.setVec3("uColor", 0.30f, 1.0f, 0.45f);

    for (auto& ch : chars) {
        if (ch.auraTimer <= 0.f) continue;

        constexpr float AURA_DUR   = 30.f;
        constexpr float FADE_IN_T  =  2.f;
        constexpr float FADE_OUT_T =  4.f;
        float ea = AURA_DUR - ch.auraTimer;
        float t = (ea < FADE_IN_T)            ? ea / FADE_IN_T
                 :(ch.auraTimer > FADE_OUT_T) ? 1.f
                 : ch.auraTimer / FADE_OUT_T;
        float pulse = 1.0f + 0.08f * std::sin(elapsed * 2.5f);

        int  drawAnim = ch.idleAnim; bool loop=true, rev=false;
        if (ch.fallAnim >= 0) {
            if (ch.state==CharState::FALLING||ch.state==CharState::GROUNDED||ch.state==CharState::RISING)
                drawAnim=ch.fallAnim;
            if (ch.state==CharState::RISING)   rev=true;
            if (ch.state==CharState::GROUNDED) loop=false;
        }
        ch.mesh.computeBones(drawAnim, ch.animTime, boneMats, loop, rev);
        aura.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(boneMats[0]));

        glm::mat4 M = glm::translate(glm::mat4(1.f), ch.worldPos);
        M = glm::rotate(M, glm::radians(ch.rotY), {0.f,1.f,0.f});
        M = glm::scale(M, glm::vec3(ch.scale));
        aura.setMat4("uModel", glm::value_ptr(M));
        aura.setFloat("uAlpha", t * 0.05f * pulse);
        ch.mesh.draw();
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
}
