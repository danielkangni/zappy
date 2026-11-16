#include "world/World.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "render/ObjLoader.hpp"
#include "GUIException.hpp"
#include <json.hpp>
using json = nlohmann::json;

#include <iostream>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <cstdlib>
#include <sys/stat.h>

static std::unordered_map<std::string, std::unique_ptr<Mesh>> g_meshes;

Mesh &getMesh(const std::string &model)
{
    auto it = g_meshes.find(model);
    if (it != g_meshes.end()) return *it->second;

    std::string path = std::string(ASSET_DIR) + model + ".obj";
    g_meshes[model] = std::make_unique<Mesh>(ObjLoader::load(path));
    std::cout << "Loaded " << model << "\n";
    return *g_meshes[model];
}

glm::vec3 colorFor(const std::string &n)
{
    if (n == "tile")                                   return {0.55f, 0.72f, 0.32f};
    if (n == "water")                                  return {0.18f, 0.48f, 0.80f};
    if (n.find("normaltree") != std::string::npos)     return {0.22f, 0.58f, 0.15f};
    if (n.find("mapletree")  != std::string::npos)     return {0.30f, 0.65f, 0.10f};
    if (n.find("palmtree")   != std::string::npos)     return {0.25f, 0.62f, 0.18f};
    if (n.find("bush")       != std::string::npos)     return {0.28f, 0.58f, 0.18f};
    if (n.find("grass")      != std::string::npos)     return {0.38f, 0.70f, 0.22f};
    if (n.find("plant")      != std::string::npos)     return {0.30f, 0.62f, 0.22f};
    if (n.find("flower_1")   != std::string::npos)     return {0.95f, 0.45f, 0.70f};
    if (n.find("flower_2")   != std::string::npos)     return {0.95f, 0.85f, 0.20f};
    if (n.find("rock")       != std::string::npos)     return {0.55f, 0.52f, 0.48f};
    if (n == "lily_pad")                               return {0.25f, 0.65f, 0.30f};
    if (n == "boat")                                   return {0.45f, 0.32f, 0.18f};
    if (n == "log")                                    return {0.50f, 0.32f, 0.15f};
    if (n == "cloud")                                  return {0.95f, 0.96f, 1.00f};
    if (n == "torch")                                  return {0.70f, 0.40f, 0.15f};
    if (n == "player_male")                            return {0.62f, 0.45f, 0.30f};
    if (n == "player_female")                          return {0.78f, 0.55f, 0.38f};
    if (n == "player_female2")                         return {0.70f, 0.50f, 0.34f};
    if (n.find("player")     != std::string::npos)     return {0.85f, 0.72f, 0.60f};
    float h = 0.f;
    for (char c : n) h = h * 31.f + c;
    h = std::fmod(std::abs(h) / 997.f, 1.f);
    float H6 = h * 6.f, C = 0.55f;
    float X = C * (1.f - std::abs(std::fmod(H6, 2.f) - 1.f));
    int s = (int)H6;
    glm::vec3 rgb;
    if      (s==0) rgb={C,X,0}; else if (s==1) rgb={X,C,0};
    else if (s==2) rgb={0,C,X}; else if (s==3) rgb={0,X,C};
    else if (s==4) rgb={X,0,C}; else           rgb={C,0,X};
    return rgb + glm::vec3(0.25f);
}

MapData loadMap(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open()) throw GUIException("Cannot open " + path);
    json doc = json::parse(f);

    MapData m;
    m.w = doc["meta"]["width"];
    m.h = doc["meta"]["height"];

    auto &sc = doc["atmosphere"]["sky_color"];
    m.skyColor = {sc[0], sc[1], sc[2]};
    auto &fc = doc["atmosphere"]["fog_color"];
    m.fogColor = {fc[0], fc[1], fc[2]};

    for (auto &t : doc["tiles"]) {
        Tile tile;
        tile.x = t["x"]; tile.y = t["y"];
        tile.ground = t["ground"];
        for (auto &p : t["props"])
            tile.props.push_back({p["model"], p["dx"], p["dz"], p["rot_y"], p["scale"]});
        if (t.contains("resources")) {
            for (int i = 0; i < 7; ++i)
                if (t["resources"].contains(RES_NAMES[i]))
                    tile.res[i] = t["resources"][RES_NAMES[i]];
        }
        m.tiles.push_back(tile);
    }
    for (auto &c : doc["atmosphere"]["clouds"]) {
        float dx = c.contains("drift_x") ? (float)c["drift_x"] : (float)c["drift_speed"];
        float dz = c.contains("drift_z") ? (float)c["drift_z"] : 0.f;
        m.clouds.push_back({c["x"], c["y"], c["z"], c["scale"], dx, dz});
    }
    return m;
}

time_t fileMtime(const std::string &p)
{
    struct stat s; return (stat(p.c_str(), &s) == 0) ? s.st_mtime : 0;
}

Mesh buildShoreMesh(const std::vector<Tile> &tiles, int dist)
{
    std::unordered_map<int, std::string> groundOf;
    for (auto &t : tiles) groundOf[t.y * 512 + t.x] = t.ground;

    auto isLand = [&](int x, int y) {
        auto it = groundOf.find(y * 512 + x);
        return it != groundOf.end() && it->second == "tile";
    };

    std::unordered_map<int, int> distMap;
    for (auto &t : tiles) {
        if (t.ground != "water") continue;
        int key = t.y * 512 + t.x;
        int dx[] = {0,0,1,-1}; int dz[] = {1,-1,0,0};
        int minD = 999;
        for (int i = 0; i < 4; ++i)
            if (isLand(t.x + dx[i], t.y + dz[i])) { minD = 1; break; }
        distMap[key] = minD;
    }
    for (int d = 2; d <= dist + 1; ++d) {
        for (auto &t : tiles) {
            if (t.ground != "water") continue;
            int key = t.y * 512 + t.x;
            if (distMap[key] < d) continue;
            int dx[] = {0,0,1,-1}; int dz[] = {1,-1,0,0};
            for (int i = 0; i < 4; ++i) {
                int nk = (t.y + dz[i]) * 512 + (t.x + dx[i]);
                if (distMap.count(nk) && distMap[nk] == d - 1) { distMap[key] = d; break; }
            }
        }
    }

    std::vector<float> verts; std::vector<unsigned int> idx; unsigned int base = 0;
    for (auto &t : tiles) {
        if (t.ground != "water") continue;
        int d = distMap.count(t.y * 512 + t.x) ? distMap[t.y * 512 + t.x] : 999;
        if (d != dist) continue;
        float x0 = t.x * TILE - TILE * 0.501f, x1 = t.x * TILE + TILE * 0.501f;
        float z0 = t.y * TILE - TILE * 0.501f, z1 = t.y * TILE + TILE * 0.501f;
        float y = -1.80f;
        verts.insert(verts.end(), {x0,y,z0, 0,1,0}); verts.insert(verts.end(), {x1,y,z0, 0,1,0});
        verts.insert(verts.end(), {x1,y,z1, 0,1,0}); verts.insert(verts.end(), {x0,y,z1, 0,1,0});
        idx.insert(idx.end(), {base,base+1,base+2, base,base+2,base+3}); base += 4;
    }
    if (verts.empty()) { verts={0,0,0,0,1,0, 0,0,0,0,1,0, 0,0,0,0,1,0}; idx={0,1,2}; }
    return Mesh(verts, idx);
}

Mesh buildGroundMesh(const std::vector<Tile> &tiles, const std::string &groundType,
                     bool cliffOnly)
{
    std::vector<float>        verts;
    std::vector<unsigned int> idx;
    unsigned int base = 0;

    const float LAND_Y  =  0.00f;
    const float WATER_Y = -1.80f;

    for (auto &t : tiles) {
        if (t.ground != groundType) continue;
        float x0 = t.x * TILE - TILE * 0.501f;
        float x1 = t.x * TILE + TILE * 0.501f;
        float z0 = t.y * TILE - TILE * 0.501f;
        float z1 = t.y * TILE + TILE * 0.501f;
        float y  = (groundType == "water") ? WATER_Y : LAND_Y;

        if (!cliffOnly) {
            verts.insert(verts.end(), {x0,y,z0, 0,1,0});
            verts.insert(verts.end(), {x1,y,z0, 0,1,0});
            verts.insert(verts.end(), {x1,y,z1, 0,1,0});
            verts.insert(verts.end(), {x0,y,z1, 0,1,0});
            idx.insert(idx.end(), {base,base+1,base+2, base,base+2,base+3});
            base += 4;
        }
        (void)cliffOnly;
    }
    if (verts.empty()) {
        verts = {0,0,0, 0,1,0, 0,0,0, 0,1,0, 0,0,0, 0,1,0};
        idx   = {0,1,2};
    }
    return Mesh(verts, idx);
}

void drawMesh(Shader &toon, Shader & , Mesh &mesh,
              const glm::mat4 &model, const glm::mat3 &normalMat, const glm::vec3 &color,
              const glm::mat4 & , const glm::mat4 & ,
              float elapsed, bool sway)
{
    toon.use();
    toon.setMat4("uModel",     glm::value_ptr(model));
    toon.setMat3("uNormalMat", glm::value_ptr(normalMat));
    toon.setVec3("uColor",     color.r, color.g, color.b);
    toon.setBool("uEmissive",  false);
    toon.setFloat("uTime",     elapsed);
    toon.setBool("uSway",      sway);
    mesh.draw();
}

Mesh buildGemMesh()
{
    const float r = 0.14f, h = 0.22f;
    std::vector<float> v;
    std::vector<unsigned int> idx;

    auto addFace = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        glm::vec3 n = glm::normalize(glm::cross(b-a, c-a));
        unsigned int base = (unsigned int)(v.size() / 6);
        for (auto &p : {a,b,c}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
                                   v.push_back(n.x); v.push_back(n.y); v.push_back(n.z); }
        idx.push_back(base); idx.push_back(base+1); idx.push_back(base+2);
    };

    glm::vec3 top(0,h,0), bot(0,-h,0);
    glm::vec3 px(r,0,0), mx(-r,0,0), pz(0,0,r), mz(0,0,-r);
    addFace(top, px, pz);  addFace(top, pz, mx);
    addFace(top, mx, mz);  addFace(top, mz, px);
    addFace(bot, pz, px);  addFace(bot, mx, pz);
    addFace(bot, mz, mx);  addFace(bot, px, mz);
    return Mesh(v, idx);
}

Mesh buildDashedRingMesh(int segs, float fill)
{
    std::vector<float>        v;
    std::vector<unsigned int> idx;
    const float PI2   = 6.28318f;
    const float INNER = 0.84f;
    const int   sub   = 10;
    unsigned int base = 0;
    for (int s = 0; s < segs; ++s) {
        float a0 = PI2 * s / segs;
        float a1 = a0 + PI2 * fill / segs;
        for (int i = 0; i <= sub; ++i) {
            float a  = a0 + (a1 - a0) * i / sub;
            float ca = std::cos(a), sa = std::sin(a);
            v.insert(v.end(), {INNER*ca, 0.f, INNER*sa, 0.f, 1.f, 0.f});
            v.insert(v.end(), {       ca, 0.f,        sa, 0.f, 1.f, 0.f});
            if (i > 0) {
                unsigned int b = base + (unsigned int)(i - 1) * 2;
                idx.insert(idx.end(), {b, b+1, b+2, b+1, b+3, b+2});
            }
        }
        base += (unsigned int)(sub + 1) * 2;
    }
    return Mesh(v, idx);
}

Mesh buildVQuadMesh()
{
    std::vector<float> v = {
        -0.5f, 0.f, 0.f, 0.f, 0.f, 1.f,
         0.5f, 0.f, 0.f, 0.f, 0.f, 1.f,
         0.5f, 1.f, 0.f, 0.f, 0.f, 1.f,
        -0.5f, 1.f, 0.f, 0.f, 0.f, 1.f,
    };
    std::vector<unsigned int> idx = {0,1,2, 0,2,3};
    return Mesh(v, idx);
}

float frnd(float a, float b)
{
    return a + (b - a) * ((float)std::rand() / (float)RAND_MAX);
}

Mesh buildRingMesh()
{
    const int   SEG   = 64;
    const float PI2   = 6.28318f;
    const float INNER = 0.6f;
    std::vector<float>        v;
    std::vector<unsigned int> idx;
    for (int i = 0; i < SEG; ++i) {
        float a = PI2 * i / SEG;
        float ca = std::cos(a), sa = std::sin(a);
        v.insert(v.end(), {INNER*ca, 0.f, INNER*sa, 0.f, 1.f, 0.f});
        v.insert(v.end(), {       ca, 0.f,        sa, 0.f, 1.f, 0.f});
    }
    for (unsigned int i = 0; i < (unsigned int)SEG; ++i) {
        unsigned int a = i*2, b = a+1;
        unsigned int c = ((i+1)%(unsigned int)SEG)*2, d = c+1;
        idx.insert(idx.end(), {a,c,b});
        idx.insert(idx.end(), {c,d,b});
    }
    return Mesh(v, idx);
}
