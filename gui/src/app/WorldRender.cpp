#include "app/WorldRender.hpp"
#include "app/Frustum.hpp"
#include "app/Islands.hpp"
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "render/TexturedMesh.hpp"
#include "world/World.hpp"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <string>

void drawGround(Shader& toon, Shader& waterShader, unsigned int waterTex,
                const glm::mat4& view, const glm::mat4& proj, float elapsed,
                const glm::vec2& sunXZ, Mesh& land, Mesh& ocean, Mesh& water,
                const MapData& map, float cx, float cz, int teamCount)
{
    (void)waterShader; (void)waterTex; (void)view; (void)proj; (void)elapsed; (void)sunXZ;
    toon.use();
    toon.setBool("uEmissive",   false);
    toon.setBool("uCircleClip", false);
    toon.setBool("uShoreBlend", false);

    glDisable(GL_CULL_FACE);
    toon.setFloat("uAlpha", 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float baseR = map.w * TILE * 0.455f;
    const int   n     = std::max(1, teamCount);

    glm::mat4 I(1.f); glm::mat3 N3(1.f);
    toon.setMat4("uModel",     glm::value_ptr(I));
    toon.setMat3("uNormalMat", glm::value_ptr(N3));
    toon.setBool("uWave", false);
    toon.setVec3("uColor", 0.06f, 0.22f, 0.55f);
    ocean.draw();

    for (int i = 0; i < n; ++i) {
        glm::vec2 c   = islandCenter(i, n, cx, cz, baseR);
        glm::vec2 off = c - glm::vec2(cx, cz);
        glm::mat4 M  = glm::translate(glm::mat4(1.f), {off.x, 0.f, off.y});
        glm::mat3 NM = glm::mat3(1.f);
        toon.setMat4("uModel",     glm::value_ptr(M));
        toon.setMat3("uNormalMat", glm::value_ptr(NM));

        toon.setVec3("uColor", 0.52f, 0.70f, 0.28f);
        toon.setBool("uWave",  false);
        land.draw();

        toon.setVec3("uColor", 0.10f, 0.40f, 0.68f);
        toon.setBool("uWave",  true);
        water.draw();
        toon.setBool("uWave", false);
    }

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void drawProps(Shader& toon, Shader& outline, const MapData& map,
               const glm::mat4& view, const glm::mat4& proj,
               float elapsed, float ambScale, float treeGlow,
               float cx, float cz, int teamCount)
{
    static const std::string kSwayModels[] = {
        "normaltree_1","mapletree_1","palmtree_1","plant_1",
        "grass_large","flower_1","flower_2","bush"
    };
    auto isVegetation = [&](const std::string& m) {
        for (auto& s : kSwayModels) if (m.find(s) != std::string::npos) return true;
        return false;
    };

    const float baseR = map.w * TILE * 0.455f;
    const int   n     = std::max(1, teamCount);
    for (int isl = 0; isl < n; ++isl) {
    glm::vec2 ioff = islandCenter(isl, n, cx, cz, baseR) - glm::vec2(cx, cz);
    for (auto& t : map.tiles) {
        float wx = t.x * TILE + ioff.x;
        float wz = t.y * TILE + ioff.y;
        for (auto& p : t.props) {
            Mesh& pm = getMesh(p.model);
            float r  = pm.boundingRadius();
            float s  = (r > 1e-4f) ? p.scale / r : p.scale;
            if (p.model == "flower_1" || p.model == "flower_2")
                s = (r > 1e-4f) ? 0.25f / r : 0.25f;

            glm::mat4 M = glm::translate(glm::mat4(1.f),
                {wx + p.dx * TILE, 0.f, wz + p.dz * TILE});
            M = glm::rotate(M, glm::radians(p.rot_y), {0.f, 1.f, 0.f});
            M = glm::scale(M, glm::vec3(s));
            glm::vec3 c = pm.centroid();
            M = glm::translate(M, {-c.x, -pm.yMin(), -c.z});
            glm::mat3 N = glm::mat3(glm::transpose(glm::inverse(M)));

            bool sway = isVegetation(p.model) || p.model == "tree_of_life";
            toon.setBool("uSway", sway);

            if (p.model == "tree_of_life" && pm.hasVertexColor()) {

                float pulse = 0.22f + 0.12f * std::sin(elapsed * 0.7f)
                            + treeGlow * 0.40f;
                toon.use();
                toon.setFloat("uAmbientStrength",  pulse);
                toon.setBool ("uUseVertexColor",   true);
                toon.setBool ("uMultiColor",        false);
                toon.setBool ("uCrystal",           false);
                toon.setBool ("uWave",              false);
                toon.setFloat("uAlpha",             1.0f);
                toon.setBool ("uCircleClip",        false);
                toon.setBool ("uShoreBlend",        false);
                toon.setMat4 ("uModel",     glm::value_ptr(M));
                toon.setMat3 ("uNormalMat", glm::value_ptr(N));
                toon.setBool ("uEmissive",  false);
                toon.setFloat("uTime",      elapsed);
                toon.setBool ("uSway",      true);
                pm.draw();
                toon.setBool ("uUseVertexColor",   false);
                toon.setFloat("uAmbientStrength",  0.22f * ambScale);
                toon.setBool ("uSway", false);
            } else {
                drawMesh(toon, outline, pm, M, N, colorFor(p.model), view, proj, elapsed, sway);
            }
        }
    }
    }
    toon.setBool("uSway", false);
}

void drawResources(Shader& egg, const std::unique_ptr<TexturedMesh>* gemTex,
                   const MapData& map, const glm::vec3& sunDir,
                   const glm::vec3& lightCol, float ambScale,
                   const glm::mat4& view, const glm::mat4& proj, float elapsed,
                   float cx, float cz, int teamCount)
{
    bool anyGem = false;
    for (int i = 1; i < 7; ++i) anyGem |= (bool)gemTex[i];
    if (!anyGem) return;

    egg.use();
    egg.setMat4 ("uView", glm::value_ptr(view));
    egg.setMat4 ("uProj", glm::value_ptr(proj));
    egg.setInt  ("uTex", 0);
    egg.setVec3 ("uDirLightDir",   sunDir.x, sunDir.y, sunDir.z);
    egg.setVec3 ("uDirLightColor", lightCol.x, lightCol.y, lightCol.z);
    egg.setVec3 ("uPointPos",   0.f, 10.f, 0.f);
    egg.setVec3 ("uPointColor", 0.f, 0.f, 0.f);
    egg.setFloat("uAmbient", 0.55f * ambScale);
    egg.setFloat("uAlpha",   1.f);
    egg.setFloat("uBright",  1.15f);
    glDisable(GL_CULL_FACE);

    const Frustum frustum = Frustum::fromVP(proj * view);
    const float spreadR = 0.30f;
    const float baseR = map.w * TILE * 0.455f;
    const int   n     = std::max(1, teamCount);
    for (int isl = 0; isl < n; ++isl) {
    glm::vec2 ioff = islandCenter(isl, n, cx, cz, baseR) - glm::vec2(cx, cz);
    for (auto& tile : map.tiles) {
        bool anyRes = false;
        for (int i = 1; i < 7; ++i) anyRes |= (tile.res[i] > 0 && gemTex[i]);
        if (!anyRes) continue;

        float wx = tile.x * TILE + TILE * 0.5f + ioff.x;
        float wz = tile.y * TILE + TILE * 0.5f + ioff.y;
        if (!frustum.sphereVisible({wx, 0.5f, wz}, TILE)) continue;
        float tileHash = float(tile.x * 17 + tile.y * 31);

        int slot = 0;
        for (int i = 1; i < 7; ++i) {
            if (tile.res[i] == 0 || !gemTex[i]) continue;
            TexturedMesh* gm = gemTex[i].get();

            float angle = slot * (2.f * 3.14159f / 7.f) + tileHash * 0.05f;
            float ox = spreadR * std::sin(angle);
            float oz = spreadR * std::cos(angle);
            float bob  = 0.35f + 0.08f * std::sin(elapsed * 1.5f + tileHash * 0.12f + i * 0.9f);
            float spin = elapsed * 1.2f + tileHash * 0.08f + i * 0.45f;
            float r = gm->boundingRadius();
            float s = (r > 1e-4f) ? 0.22f / r : 0.22f;

            glm::mat4 M = glm::translate(glm::mat4(1.f), {wx+ox, bob, wz+oz});
            M = glm::rotate(M, spin, {0.f, 1.f, 0.f});
            M = glm::scale(M, glm::vec3(s));
            M = glm::translate(M, -gm->centroid());
            glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));

            egg.setMat4("uModel",     glm::value_ptr(M));
            egg.setMat3("uNormalMat", glm::value_ptr(N3));
            gm->draw();
            ++slot;
        }
    }
    }
    glEnable(GL_CULL_FACE);
}

void drawFood(Shader& egg, const std::unique_ptr<TexturedMesh>* foodMesh,
              const MapData& map, const glm::vec3& sunDir,
              const glm::vec3& lightCol, float ambScale,
              const glm::mat4& view, const glm::mat4& proj, float elapsed,
              float cx, float cz, int teamCount)
{
    if (!(foodMesh[0] || foodMesh[1] || foodMesh[2])) return;
    egg.use();
    egg.setMat4 ("uView", glm::value_ptr(view));
    egg.setMat4 ("uProj", glm::value_ptr(proj));
    egg.setInt  ("uTex", 0);
    egg.setVec3 ("uDirLightDir",   sunDir.x, sunDir.y, sunDir.z);
    egg.setVec3 ("uDirLightColor", lightCol.x, lightCol.y, lightCol.z);
    egg.setVec3 ("uPointPos",   0.f, 10.f, 0.f);
    egg.setVec3 ("uPointColor", 0.f, 0.f, 0.f);
    egg.setFloat("uAmbient", 0.55f * ambScale);
    egg.setFloat("uAlpha",   1.f);
    egg.setFloat("uBright",  1.f);
    glDisable(GL_CULL_FACE);
    const Frustum frustum = Frustum::fromVP(proj * view);
    const float baseR = map.w * TILE * 0.455f;
    const int   n     = std::max(1, teamCount);
    for (int isl = 0; isl < n; ++isl) {
    glm::vec2 ioff = islandCenter(isl, n, cx, cz, baseR) - glm::vec2(cx, cz);
    for (auto& tile : map.tiles) {
        if (tile.res[0] <= 0) continue;
        float wx = tile.x * TILE + TILE * 0.5f + ioff.x;
        float wz = tile.y * TILE + TILE * 0.5f + ioff.y;
        if (!frustum.sphereVisible({wx, 0.5f, wz}, TILE)) continue;
        int v = ((tile.x * 7 + tile.y * 13) % 3 + 3) % 3;
        TexturedMesh* fm = foodMesh[v] ? foodMesh[v].get()
                         : foodMesh[0] ? foodMesh[0].get()
                         : foodMesh[1] ? foodMesh[1].get() : foodMesh[2].get();
        if (!fm) continue;
        float r  = fm->boundingRadius();
        float s  = (r > 1e-4f) ? 0.175f / r : 0.175f;
        float bob = 0.30f + 0.05f * std::sin(elapsed * 1.5f + (tile.x * 17 + tile.y * 31) * 0.1f);
        glm::mat4 M = glm::translate(glm::mat4(1.f), {wx, bob, wz});
        M = glm::rotate(M, elapsed * 0.5f, {0.f, 1.f, 0.f});
        M = glm::scale(M, glm::vec3(s));
        M = glm::translate(M, -fm->centroid());
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        egg.setMat4("uModel",     glm::value_ptr(M));
        egg.setMat3("uNormalMat", glm::value_ptr(N3));
        fm->draw();
    }
    }
    glEnable(GL_CULL_FACE);
}

void drawClouds(Shader& toon, const MapData& map, float elapsed)
{
    Mesh& cloudMesh = getMesh("cloud");
    for (auto& c : map.clouds) {
        float x = c.x * TILE + elapsed * c.drift_x * TILE;
        float y = c.y;
        float z = c.z * TILE + elapsed * c.drift_z * TILE;

        float r = cloudMesh.boundingRadius();
        float s = (r > 1e-4f) ? c.scale / r : c.scale;

        glm::mat4 M = glm::translate(glm::mat4(1.f), {x, y, z});
        M = glm::scale(M, glm::vec3(s));
        M = glm::translate(M, -cloudMesh.centroid());
        glm::mat3 N = glm::mat3(glm::transpose(glm::inverse(M)));

        toon.use();
        toon.setMat4("uModel",     glm::value_ptr(M));
        toon.setMat3("uNormalMat", glm::value_ptr(N));
        toon.setVec3("uColor",     0.96f, 0.97f, 1.0f);
        toon.setBool("uEmissive",  false);
        cloudMesh.draw();
    }
}
