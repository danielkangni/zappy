#include <GL/glew.h>
#include "app/PlayerRender.hpp"
#include "app/Islands.hpp"
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "render/TexturedMesh.hpp"
#include "net/GuiClient.hpp"
#include "world/World.hpp"
#include "hud/HudOverlay.hpp"
#include "app/Constellation.hpp"
#include "app/Particle.hpp"
#include "app/Frustum.hpp"
#include "audio/AudioManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

int tierOf(const PlayerTier tiers[4], int level) {
    int t = std::min(std::max((level - 1) / 2, 0), 3);
    for (int k = 0; k < 4 && !tiers[t].loaded; ++k) t = (t + 1) % 4;
    return t;
}

static float orientationYaw(int o) {
    switch (o) { case 1: return 180.f; case 2: return 90.f;
                 case 3: return 0.f;   case 4: return 270.f; }
    return 0.f;
}

static float frand(float a, float b) {
    return a + (b - a) * ((float)std::rand() / (float)RAND_MAX);
}

static glm::vec2 worldRaw(PlayerRenderCtx& ctx, const GuiPlayer& p) {
    if (ctx.testMode)
        return { p.x * TILE + TILE * 0.5f, p.y * TILE + TILE * 0.5f };
    const GuiState& gs = ctx.netClient.state();
    int W = gs.sizeKnown ? gs.width : ctx.map.w;
    int H = gs.sizeKnown ? gs.height : ctx.map.h;
    glm::vec2 off(0.f, 0.f);

    const float insetT = 7.f;
    float landX = std::max(1.f, ctx.map.w - 2.f * insetT);
    float landY = std::max(1.f, ctx.map.h - 2.f * insetT);
    float wx = (insetT + (p.x + 0.5f) * landX / W) * TILE;
    float wz = (insetT + (p.y + 0.5f) * landY / H) * TILE;
    return { wx + off.x, wz + off.y };
}

static float scaleOf(PlayerRenderCtx& ctx, int level) {
    int L = level < 1 ? 1 : (level > 8 ? 8 : level);
    return ctx.tiers[tierOf(ctx.tiers, level)].scale * (0.44f + 0.051f * (L - 1));
}

static glm::mat4 playerModel(PlayerRenderCtx& ctx, const GuiPlayer& p) {
    glm::vec2 w = ctx.playerAnims[p.id].curWorld;
    glm::mat4 M = glm::translate(glm::mat4(1.f), {w.x, 0.f, w.y});
    M = glm::rotate(M, glm::radians(orientationYaw(p.orientation)), {0.f,1.f,0.f});
    M = glm::scale(M, glm::vec3(scaleOf(ctx, p.level)));
    return M;
}

static void updatePlayerStates(PlayerRenderCtx& ctx,
                               const std::vector<const GuiPlayer*>& drawList,
                               float dt) {
    const float MOVE_DUR = 0.50f;

    static const float WALK_CYCLE_DIST = std::getenv("ZAPPY_STRIDE")
        ? std::max(0.2f, (float)std::atof(std::getenv("ZAPPY_STRIDE"))) : TILE;
    for (const GuiPlayer* pp : drawList) {
        const GuiPlayer& p = *pp;
        PlayerAnim& pa = ctx.playerAnims[p.id];
        glm::vec2 target = worldRaw(ctx, p);
        if (!pa.posInit) { pa.curWorld = pa.prevWorld = target; pa.posInit = true; }
        pa.freezeT = std::max(0.f, pa.freezeT - dt);
        bool frozen = pa.freezeT > 0.f;
        if (!frozen && (p.x != pa.px || p.y != pa.py)) {
            const GuiState& gs2 = ctx.netClient.state();
            int W = gs2.sizeKnown ? gs2.width : ctx.map.w;
            int H = gs2.sizeKnown ? gs2.height : ctx.map.h;

            bool wrap = std::abs(p.x - pa.px) > W / 2 ||
                        std::abs(p.y - pa.py) > H / 2;
            bool snap = wrap || const_cast<GuiPlayer&>(p).ejected;
            const_cast<GuiPlayer&>(p).ejected = false;
            if (snap) {
                pa.prevWorld = target; pa.curWorld = target; pa.moveLerp = 1.f;
            } else {
                pa.prevWorld = pa.curWorld; pa.moveLerp = 0.f; pa.moveT = 0.5f;
            }
            pa.px = p.x; pa.py = p.y;
        }
        if (!frozen && p.orientation != pa.po) {
            if (pa.po != -1) {
                int diff = (p.orientation - pa.po + 4) % 4;
                pa.turnAnim = (diff == 3) ? 3 : 4;
                pa.turnT = 0.4f;
            }
            pa.po = p.orientation;
        }
        glm::vec2 beforeC = pa.curWorld;
        if (!frozen) {
            pa.moveLerp = std::min(1.f, pa.moveLerp + dt / MOVE_DUR);
            float sm = pa.moveLerp * pa.moveLerp * (3.f - 2.f * pa.moveLerp);
            pa.curWorld = glm::mix(pa.prevWorld, target, sm);
        }
        pa.moveT = std::max(0.f, pa.moveT - dt);
        pa.turnT = std::max(0.f, pa.turnT - dt);
        static const float ANIM_BLEND_DUR = 0.18f;
        int a = 0;
        if (frozen)                 a = 0;
        else if (pa.moveLerp < 1.f) a = 1;
        else if (pa.turnT > 0.f)    a = pa.turnAnim;
        if (a >= ctx.tiers[tierOf(ctx.tiers, p.level)].mesh.animCount()) a = 0;
        if (a != pa.anim) {
            pa.prevAnim     = pa.anim;
            pa.prevAnimTime = pa.animTime;
            pa.blendT       = ANIM_BLEND_DUR;
        }
        pa.blendT = std::max(0.f, pa.blendT - dt);
        pa.anim = a;

        if (a == 1) {
            float frameDist = glm::length(pa.curWorld - beforeC);
            float cyc = ctx.tiers[tierOf(ctx.tiers, p.level)].mesh.animDuration(1);
            pa.animTime += (frameDist / WALK_CYCLE_DIST) * cyc;
        } else {
            pa.animTime += dt;
        }
        if (p.level != pa.prevLevel) {
            if (pa.prevLevel >= 0 && p.level >= 1) {
                pa.evoT = ctx.evoDur;
                pa.freezeT = ctx.evoDur;
                ctx.gameAudio.play(pa.prevLevel == 0 ? "hatch" : "levelup", 90.f);
                float wx = pa.curWorld.x, wz = pa.curWorld.y;
                for (int i = 0; i < 30; ++i) {
                    Spark s;
                    glm::vec3 d(frand(-1, 1), frand(0.2f, 1.6f), frand(-1, 1));
                    s.pos = {wx, frand(0.3f, 2.2f), wz};
                    s.vel = d * frand(1.2f, 3.2f);
                    s.maxLife = frand(0.6f, 1.3f); s.life = s.maxLife;
                    s.size = frand(0.07f, 0.17f);
                    s.phase = frand(0.f, 6.28f); s.twFreq = frand(12.f, 28.f);
                    s.color = glm::mix(glm::vec3(0.6f, 0.8f, 1.0f),
                                       glm::vec3(1.0f, 1.0f, 0.85f), frand(0.f, 1.f));
                    ctx.sparks.push_back(s);
                }
            }
            pa.prevLevel = p.level;
        }
        pa.evoT = std::max(0.f, pa.evoT - dt);
    }
}

static void drawPlayerPasses(PlayerRenderCtx& ctx,
                             const std::vector<const GuiPlayer*>& drawList,
                             Shader& skinShader, Shader& auraShader,
                             Shader& toon, Shader& starShader, Shader& eggShader,
                             unsigned sparkVAO,
                             const glm::mat4& view, const glm::mat4& proj,
                             const glm::vec3& camPos, const glm::vec3& sunDir,
                             const glm::vec3& lightCol, float ambScale,
                             float nightF, float elapsed, float dt) {
    glm::mat4 pBones[MAX_BONES];

    const Frustum frustum = Frustum::fromVP(proj * view);
    const float CULL_R = TILE * 2.0f;
    auto cullW = [&](const glm::vec2& w) {
        return !frustum.sphereVisible(glm::vec3(w.x, 0.5f, w.y), CULL_R);
    };

    skinShader.use();
    skinShader.setMat4("uView", glm::value_ptr(view));
    skinShader.setMat4("uProj", glm::value_ptr(proj));
    skinShader.setVec3("uCamPos",          camPos.x, camPos.y, camPos.z);
    skinShader.setVec3("uDirLightDir",     sunDir.x, sunDir.y, sunDir.z);
    skinShader.setVec3("uDirLightColor",   lightCol.x, lightCol.y, lightCol.z);
    skinShader.setVec3("uPointLight0Color",0.f,0.f,0.f);
    skinShader.setVec3("uPointLight1Color",0.f,0.f,0.f);
    skinShader.setFloat("uShininess",      28.f);
    skinShader.setFloat("uAmbientStrength",0.30f * ambScale);
    skinShader.setFloat("uAlpha",          1.0f);
    skinShader.setFloat("uTime",           elapsed);
    skinShader.setBool ("uEmissive",       false);
    skinShader.setBool ("uCrystal",        false);
    skinShader.setBool ("uWave",           false);
    skinShader.setBool ("uSway",           false);
    skinShader.setBool ("uCircleClip",     false);
    skinShader.setBool ("uShoreBlend",     false);
    skinShader.setFloat("uShoreInnerR",    0.f);
    skinShader.setFloat("uShoreWidth",     1.f);
    skinShader.setBool ("uUseTexture",     true);
    skinShader.setBool ("uAlphaCutout",    false);
    skinShader.setInt  ("uTexture",        0);
    skinShader.setBool ("uUseVertexColor", false);
    skinShader.setBool ("uMultiColor",     false);
    for (const GuiPlayer* pp : drawList) {
        const GuiPlayer& p = *pp;
        if (p.level == 0) continue;
        if (cullW(ctx.playerAnims[p.id].curWorld)) continue;
        SkinnedMesh& m = ctx.tiers[tierOf(ctx.tiers, p.level)].mesh;
        static const float ANIM_BLEND_DUR = 0.18f;
        const PlayerAnim& pa2 = ctx.playerAnims[p.id];
        m.computeBones(pa2.anim, pa2.animTime, pBones, true, false);
        if (pa2.blendT > 0.f) {
            glm::mat4 prevBones[MAX_BONES];
            m.computeBones(pa2.prevAnim, pa2.prevAnimTime, prevBones, true, false);
            float alpha = 1.f - pa2.blendT / ANIM_BLEND_DUR;
            for (int i = 0; i < MAX_BONES; ++i)
                pBones[i] = prevBones[i] * (1.f - alpha) + pBones[i] * alpha;
        }
        skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
        glm::mat4 M = playerModel(ctx, p);
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skinShader.setMat4("uModel",     glm::value_ptr(M));
        skinShader.setMat3("uNormalMat", glm::value_ptr(N3));
        m.draw();
    }

    if (ctx.adminMode && ctx.haveMew) {
        ctx.mewMesh.computeBones(0, ctx.mewAnimTime, pBones, true, false);
        skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));

        float bobAmp = ctx.mewMoving ? 0.12f : 0.32f;
        float by = ctx.mewPos.y + std::sin(elapsed * 2.2f) * bobAmp;
        glm::mat4 M = glm::translate(glm::mat4(1.f), glm::vec3(ctx.mewPos.x, by, ctx.mewPos.z));
        M = glm::rotate(M, ctx.mewYaw, glm::vec3(0, 1, 0));
        M = glm::scale(M, glm::vec3(ctx.mewScale));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skinShader.setMat4("uModel",     glm::value_ptr(M));
        skinShader.setMat3("uNormalMat", glm::value_ptr(N3));
        ctx.mewMesh.draw();
    }

    if (ctx.haveCharz) {
        ctx.charzMesh.computeBones(ctx.charzAnimIdx, ctx.charzAnimTime, pBones, true, false, ctx.charzStripRoot);
        skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));

        skinShader.setBool ("uUseTexture",     true);
        skinShader.setBool ("uAlphaCutout",    false);
        skinShader.setInt  ("uTexture",        0);
        skinShader.setBool ("uUseVertexColor", false);
        skinShader.setBool ("uMultiColor",     false);
        skinShader.setBool ("uEmissive",       false);
        skinShader.setBool ("uCrystal",        false);

        skinShader.setFloat("uAmbientStrength",0.32f * ambScale);
        skinShader.setFloat("uShininess",      24.f);
        skinShader.setFloat("uAlpha",          1.f);
        skinShader.setVec3 ("uColor",          1.f, 1.f, 1.f);
        skinShader.setBool ("uCircleClip",     false);
        skinShader.setBool ("uShoreBlend",     false);
        glDisable(GL_CULL_FACE);
        float by = ctx.charzPos.y + std::sin(elapsed * 1.6f) * ctx.charzBobAmp;
        glm::mat4 M = glm::translate(glm::mat4(1.f), glm::vec3(ctx.charzPos.x, by, ctx.charzPos.z));
        M = glm::rotate(M, ctx.charzYaw, glm::vec3(0, 1, 0));
        M = glm::rotate(M, glm::radians(90.f), glm::vec3(1, 0, 0));
        M = glm::scale(M, glm::vec3(ctx.charzScale));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skinShader.setMat4("uModel",     glm::value_ptr(M));
        skinShader.setMat3("uNormalMat", glm::value_ptr(N3));
        ctx.charzMesh.draw();
        glEnable(GL_CULL_FACE);
    }

    if (ctx.haveMoon && nightF > 0.01f) {
        ctx.moonMesh.computeBones(0, 0.f, pBones, true, false);
        skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
        glm::vec3 mp = camPos + ctx.moonDir * 1600.f;
        glm::mat4 M  = glm::translate(glm::mat4(1.f), mp);
        M = glm::scale(M, glm::vec3(5.0f));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skinShader.setMat4 ("uModel",           glm::value_ptr(M));
        skinShader.setMat3 ("uNormalMat",       glm::value_ptr(N3));
        skinShader.setBool ("uUseTexture",      true);
        skinShader.setFloat("uAmbientStrength", 1.5f);
        skinShader.setVec3 ("uDirLightColor",   0.f, 0.f, 0.f);
        skinShader.setFloat("uAlpha",           nightF);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        ctx.moonMesh.draw();
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        skinShader.setFloat("uAlpha", 1.0f);
    }

    const std::string& selTeam = ctx.hud.selectedTeam();
    if (!selTeam.empty()) {
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        auraShader.use();
        auraShader.setMat4("uView", glm::value_ptr(view));
        auraShader.setMat4("uProj", glm::value_ptr(proj));
        int ti = 0;
        { const GuiState& gs = ctx.netClient.state();
          for (int k = 0; k < (int)gs.teams.size(); ++k)
              if (gs.teams[k] == selTeam) ti = k; }
        glm::vec3 tc = HudOverlay::teamColor3(ti);
        float gp = 0.55f + 0.45f * std::sin(elapsed * 4.f);
        for (const GuiPlayer* pp : drawList) {
            const GuiPlayer& p = *pp;
            if (p.team != selTeam || p.level < 1) continue;
            SkinnedMesh& m = ctx.tiers[tierOf(ctx.tiers, p.level)].mesh;
            m.computeBones(ctx.playerAnims[p.id].anim, ctx.playerAnims[p.id].animTime,
                           pBones, true, false);
            auraShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
            auraShader.setVec3 ("uColor", tc.r, tc.g, tc.b);
            auraShader.setFloat("uAlpha", 0.30f + 0.35f * gp);
            glm::mat4 M = glm::scale(playerModel(ctx, p), glm::vec3(1.06f));
            auraShader.setMat4("uModel", glm::value_ptr(M));
            m.draw();
        }
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }

    if (ctx.eggMeshTex) {
        float r  = ctx.eggMeshTex->boundingRadius();
        float es = (r > 1e-4f) ? 0.15f / r : 0.15f;
        glm::vec3 ec = ctx.eggMeshTex->centroid();
        eggShader.use();
        eggShader.setMat4 ("uView", glm::value_ptr(view));
        eggShader.setMat4 ("uProj", glm::value_ptr(proj));
        eggShader.setInt  ("uTex", 0);
        eggShader.setVec3 ("uDirLightDir",   sunDir.x, sunDir.y, sunDir.z);
        eggShader.setVec3 ("uDirLightColor", lightCol.x, lightCol.y, lightCol.z);
        eggShader.setVec3 ("uPointPos",   0.f, 10.f, 0.f);
        eggShader.setVec3 ("uPointColor", 0.f, 0.f, 0.f);
        eggShader.setFloat("uAmbient", 0.45f * ambScale);
        eggShader.setFloat("uAlpha",   1.f);
        eggShader.setFloat("uBright",  1.f);
        glDisable(GL_CULL_FACE);
        for (const GuiPlayer* pp : drawList) {
            const GuiPlayer& p = *pp;
            if (p.level != 0) continue;
            glm::vec2 ew = ctx.playerAnims[p.id].curWorld;
            if (cullW(ew)) continue;
            float wx  = ew.x;
            float wz  = ew.y;
            float wob = std::sin(elapsed * 3.f + p.id) * 0.06f;
            glm::mat4 M = glm::translate(glm::mat4(1.f), {wx, es * r, wz});
            M = glm::rotate(M, wob, {0.f, 0.f, 1.f});
            M = glm::scale(M, glm::vec3(es));
            M = glm::translate(M, -ec);
            glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
            eggShader.setMat4("uModel",     glm::value_ptr(M));
            eggShader.setMat3("uNormalMat", glm::value_ptr(N3));
            ctx.eggMeshTex->draw();
        }

        if (!ctx.testMode) {
            const GuiState& gs = ctx.netClient.state();
            int W = gs.sizeKnown ? gs.width : ctx.map.w;
            int H = gs.sizeKnown ? gs.height : ctx.map.h;
            const float insetT = 7.f;
            float landX = std::max(1.f, ctx.map.w - 2.f * insetT);
            float landY = std::max(1.f, ctx.map.h - 2.f * insetT);
            for (const auto& kv : gs.eggs) {
                const GuiEgg& egg = kv.second;
                float wx = (insetT + (egg.x + 0.5f) * landX / W) * TILE;
                float wz = (insetT + (egg.y + 0.5f) * landY / H) * TILE;
                if (cullW({wx, wz})) continue;
                float wob = std::sin(elapsed * 3.f + egg.id) * 0.06f;
                glm::mat4 M = glm::translate(glm::mat4(1.f), {wx, es * r, wz});
                M = glm::rotate(M, wob, {0.f, 0.f, 1.f});
                M = glm::scale(M, glm::vec3(es));
                M = glm::translate(M, -ec);
                glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
                eggShader.setMat4("uModel",     glm::value_ptr(M));
                eggShader.setMat3("uNormalMat", glm::value_ptr(N3));
                ctx.eggMeshTex->draw();
            }
        }
        glEnable(GL_CULL_FACE);
    }

    static Mesh levelRing = buildRingMesh();
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    toon.use();
    toon.setBool ("uEmissive", true);
    toon.setBool ("uCircleClip", false);
    toon.setBool ("uShoreBlend", false);
    float ringPulse = 0.70f + 0.30f * std::sin(elapsed * 3.f);
    for (const GuiPlayer* pp : drawList) {
        const GuiPlayer& p = *pp;
        if (p.level == 0) continue;
        glm::vec2 rw0 = ctx.playerAnims[p.id].curWorld;
        if (cullW(rw0)) continue;
        glm::vec3 base = (p.level % 2 == 1) ? glm::vec3(0.60f, 0.20f, 1.0f)
                                            : glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 col = base * ringPulse;
        glm::vec2 rw = ctx.playerAnims[p.id].curWorld;
        float wx = rw.x;
        float wz = rw.y;
        glm::mat4 M = glm::translate(glm::mat4(1.f), {wx, 0.06f, wz});
        M = glm::scale(M, glm::vec3(TILE * 0.5f, 1.f, TILE * 0.5f));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        toon.setMat4("uModel",     glm::value_ptr(M));
        toon.setMat3("uNormalMat", glm::value_ptr(N3));
        toon.setVec3("uColor",     col.r, col.g, col.b);
        levelRing.draw();
    }
    toon.setBool("uEmissive", false);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    auraShader.use();
    auraShader.setMat4("uView", glm::value_ptr(view));
    auraShader.setMat4("uProj", glm::value_ptr(proj));
    for (const GuiPlayer* pp : drawList) {
        const GuiPlayer& p = *pp;
        PlayerAnim& pa = ctx.playerAnims[p.id];
        if (pa.evoT <= 0.f || p.level < 1) continue;
        float e    = 1.f - pa.evoT / ctx.evoDur;
        float glow = std::sin(e * 3.14159f);
        SkinnedMesh& m = ctx.tiers[tierOf(ctx.tiers, p.level)].mesh;
        m.computeBones(0, pa.animTime, pBones, true, false);
        auraShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
        auraShader.setVec3 ("uColor", 0.85f, 0.95f, 1.0f);
        auraShader.setFloat("uAlpha", glow * 0.95f);
        glm::mat4 M = playerModel(ctx, p);
        M = glm::scale(M, glm::vec3(1.0f + 0.14f * glow));
        auraShader.setMat4("uModel", glm::value_ptr(M));
        m.draw();
    }

    for (auto& s : ctx.sparks) {
        s.vel.y -= 1.5f * dt;
        s.pos += s.vel * dt;
        s.life -= dt;
    }
    ctx.sparks.erase(std::remove_if(ctx.sparks.begin(), ctx.sparks.end(),
        [](const Spark& s) { return s.life <= 0.f; }), ctx.sparks.end());
    if (!ctx.sparks.empty()) {
        starShader.use();
        starShader.setMat4("uView", glm::value_ptr(view));
        starShader.setMat4("uProj", glm::value_ptr(proj));
        glBlendFunc(GL_ONE, GL_ONE);
        glBindVertexArray(sparkVAO);
        for (auto& s : ctx.sparks) {
            float k  = s.life / s.maxLife;
            float tw = 0.55f + 0.45f * std::sin(elapsed * s.twFreq + s.phase);
            starShader.setVec3 ("uCenter", s.pos.x, s.pos.y, s.pos.z);
            starShader.setFloat("uSize",   s.size);
            starShader.setVec3 ("uColor",  s.color.r, s.color.g, s.color.b);
            starShader.setFloat("uIntensity", (0.4f + 0.9f * k) * tw);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
        glBindVertexArray(0);
    }

    if (nightF > 0.02f)
        ctx.constellation.renderNightSky(starShader, sparkVAO, view, proj,
                                         camPos, elapsed, nightF);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void updateAndDrawPlayers(PlayerRenderCtx& ctx,
                          Shader& skinShader, Shader& auraShader,
                          Shader& toon, Shader& starShader, Shader& eggShader,
                          unsigned sparkVAO,
                          const glm::mat4& view, const glm::mat4& proj,
                          const glm::vec3& camPos, const glm::vec3& sunDir,
                          const glm::vec3& lightCol, float ambScale,
                          float nightF, float elapsed, float dt) {
    bool havePlayerMesh = ctx.tiers[0].loaded || ctx.tiers[1].loaded ||
                          ctx.tiers[2].loaded || ctx.tiers[3].loaded;
    if (!havePlayerMesh || !(ctx.testMode || ctx.netConnected)) return;

    std::vector<const GuiPlayer*> drawList;
    if (ctx.testMode) {
        for (const auto& p : ctx.testPlayers) drawList.push_back(&p);
    } else {
        for (const auto& kv : ctx.netClient.state().players) drawList.push_back(&kv.second);
    }

    updatePlayerStates(ctx, drawList, dt);
    drawPlayerPasses(ctx, drawList, skinShader, auraShader, toon, starShader, eggShader,
                     sparkVAO, view, proj, camPos, sunDir, lightCol, ambScale,
                     nightF, elapsed, dt);
}
