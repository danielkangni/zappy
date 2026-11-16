#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "render/SkinnedMesh.hpp"

class Shader;
class GuiClient;
class HudOverlay;
class Constellation;
class AudioManager;
class TexturedMesh;
struct GuiPlayer;
struct MapData;
struct Spark;

struct PlayerTier { SkinnedMesh mesh; float scale = 0.05f; bool loaded = false; };

int tierOf(const PlayerTier tiers[4], int level);

struct PlayerAnim {
    float animTime = 0.f;
    int   px = -9999, py = -9999, po = -1;
    float moveT = 0.f, turnT = 0.f;
    int   turnAnim = 3;
    int   prevLevel = -1;
    float evoT = 0.f;
    float freezeT = 0.f;
    glm::vec2 curWorld{0.f}, prevWorld{0.f};
    float moveLerp = 1.f;
    bool  posInit = false;
    int   anim = 0;
    int   prevAnim = 0;
    float prevAnimTime = 0.f;
    float blendT = 0.f;
};

struct PlayerRenderCtx {
    PlayerTier (&tiers)[4];
    std::unordered_map<int, PlayerAnim>& playerAnims;
    std::vector<Spark>& sparks;

    bool testMode;
    bool netConnected;
    const std::vector<GuiPlayer>& testPlayers;
    GuiClient& netClient;
    const MapData& map;
    float evoDur;
    AudioManager& gameAudio;

    bool adminMode, haveMew;
    SkinnedMesh& mewMesh;
    float mewAnimTime; bool mewMoving;
    glm::vec3 mewPos; float mewYaw; float mewScale;

    bool haveCharz;
    SkinnedMesh& charzMesh;
    int   charzAnimIdx;
    float charzAnimTime;
    bool  charzStripRoot;
    float charzBobAmp;
    glm::vec3 charzPos; float charzYaw; float charzScale;

    bool haveMoon;
    SkinnedMesh& moonMesh;
    glm::vec3 moonDir;

    TexturedMesh* eggMeshTex;

    HudOverlay& hud;
    Constellation& constellation;
};

void updateAndDrawPlayers(PlayerRenderCtx& ctx,
                          Shader& skinShader, Shader& auraShader,
                          Shader& toon, Shader& starShader, Shader& eggShader,
                          unsigned sparkVAO,
                          const glm::mat4& view, const glm::mat4& proj,
                          const glm::vec3& camPos, const glm::vec3& sunDir,
                          const glm::vec3& lightCol, float ambScale,
                          float nightF, float elapsed, float dt);
