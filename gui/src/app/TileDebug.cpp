#include "app/TileDebug.hpp"
#include "world/World.hpp"
#include "net/GuiState.hpp"

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>

static const char* GRID_VERT = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP;
void main() { gl_Position = uVP * vec4(aPos, 1.0); }
)GLSL";

static const char* GRID_FRAG = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)GLSL";

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

static GLuint buildGridProgram()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER,   GRID_VERT);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, GRID_FRAG);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

static float tileWorldX(int sx, int mapW, int srvW)
{
    const int insetT = 7;
    float landX = (float)std::max(1, mapW - 2 * insetT);
    return (insetT + (float)sx * landX / (float)std::max(1, srvW)) * TILE;
}
static float tileWorldZ(int sy, int mapH, int srvH)
{
    const int insetT = 7;
    float landY = (float)std::max(1, mapH - 2 * insetT);
    return (insetT + (float)sy * landY / (float)std::max(1, srvH)) * TILE;
}

void drawTileGridGL(const glm::mat4& view, const glm::mat4& proj,
                    const MapData& map, const GuiState& gs, float cx, float cz)
{
    static GLuint prog = 0;
    static GLuint vao  = 0;
    static GLuint vbo  = 0;
    static int    lastW = -1, lastH = -1, lastSW = -1, lastSH = -1;
    static int    lineCount = 0;

    if (!prog) prog = buildGridProgram();
    (void)cx; (void)cz;

    if (!gs.sizeKnown) return;

    const int SW = gs.width, SH = gs.height;
    const int MW = map.w,    MH = map.h;

    if (SW != lastSW || SH != lastSH || MW != lastW || MH != lastH) {
        std::vector<float> verts;
        verts.reserve(((SW+1) + (SH+1)) * 6);
        const float Y = 0.05f;

        float x0 = tileWorldX(0,  MW, SW), x1 = tileWorldX(SW, MW, SW);
        float z0 = tileWorldZ(0,  MH, SH), z1 = tileWorldZ(SH, MH, SH);

        for (int gy = 0; gy <= SH; ++gy) {
            float z = tileWorldZ(gy, MH, SH);
            verts.insert(verts.end(), {x0, Y, z});
            verts.insert(verts.end(), {x1, Y, z});
        }

        for (int gx = 0; gx <= SW; ++gx) {
            float x = tileWorldX(gx, MW, SW);
            verts.insert(verts.end(), {x, Y, z0});
            verts.insert(verts.end(), {x, Y, z1});
        }
        lineCount = (SW + 1 + SH + 1);

        if (!vao) { glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); }
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)),
                     verts.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        lastW = MW; lastH = MH; lastSW = SW; lastSH = SH;
    }

    glm::mat4 vp = proj * view;
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
    glUniform4f(glGetUniformLocation(prog, "uColor"), 1.f, 1.f, 0.4f, 0.55f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, lineCount * 2);
    glBindVertexArray(0);

    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

static const char* RES_FULL[7]  = {"Food","Linemate","Deraumere","Sibur",
                                    "Mendiane","Phiras","Thystame"};

static bool mouseTile(const glm::mat4& view, const glm::mat4& proj,
                      unsigned winW, unsigned winH, int mx, int my,
                      const MapData& map, const GuiState& gs, int& tx, int& ty)
{
    float ndcX =  (2.f * mx) / (float)winW - 1.f;
    float ndcY = -(2.f * my) / (float)winH + 1.f;

    glm::mat4 invVP = glm::inverse(proj * view);
    glm::vec4 near4 = invVP * glm::vec4(ndcX, ndcY, -1.f, 1.f);
    glm::vec4 far4  = invVP * glm::vec4(ndcX, ndcY,  1.f, 1.f);
    glm::vec3 ro = glm::vec3(near4) / near4.w;
    glm::vec3 rd = glm::normalize(glm::vec3(far4) / far4.w - ro);

    if (std::abs(rd.y) < 1e-5f) return false;
    float t = -ro.y / rd.y;
    if (t < 0.f) return false;

    glm::vec3 hit = ro + rd * t;

    const int SW = gs.width, SH = gs.height;

    const int insetT = 7;
    float landX = (float)std::max(1, map.w - 2 * insetT);
    float landY = (float)std::max(1, map.h - 2 * insetT);
    float mapFracX = hit.x / TILE - insetT;
    float mapFracZ = hit.z / TILE - insetT;
    tx = (int)std::floor(mapFracX * SW / landX);
    ty = (int)std::floor(mapFracZ * SH / landY);
    return tx >= 0 && ty >= 0;
}

void drawTileGridHUD(const glm::mat4& view, const glm::mat4& proj,
                     const MapData& map, const GuiState& gs,
                     float cx, float cz,
                     sf::RenderWindow& win, const sf::Font& font,
                     unsigned winW, unsigned winH,
                     int mouseX, int mouseY)
{
    (void)cx; (void)cz;

    if (!gs.sizeKnown) return;

    int tx = -1, ty = -1;
    if (!mouseTile(view, proj, winW, winH, mouseX, mouseY, map, gs, tx, ty))
        return;

    float wx = (tileWorldX(tx, map.w, gs.width) + tileWorldX(tx+1, map.w, gs.width)) * 0.5f;
    float wz = (tileWorldZ(ty, map.h, gs.height) + tileWorldZ(ty+1, map.h, gs.height)) * 0.5f;
    glm::vec4 clip = proj * view * glm::vec4(wx, 0.15f, wz, 1.f);
    if (clip.w <= 0.f) return;
    float sx = (clip.x / clip.w * 0.5f + 0.5f) * (float)winW;
    float sy = (1.f - (clip.y / clip.w * 0.5f + 0.5f)) * (float)winH;

    std::vector<std::string> lines;
    lines.push_back("Tile (" + std::to_string(tx) + ", " + std::to_string(ty) + ")");

    if (gs.sizeKnown && gs.inBounds(tx, ty)) {
        const auto& tile = gs.tileAt(tx, ty);
        for (int i = 0; i < 7; ++i) {
            if (tile.res[i] > 0) {
                lines.push_back(std::string(RES_FULL[i]) + ": " +
                                std::to_string(tile.res[i]));
            }
        }
    }

    if (gs.sizeKnown) {
        for (auto& [id, p] : gs.players) {
            if (p.x == tx && p.y == ty) {
                lines.push_back("Player #" + std::to_string(id) +
                                " [" + p.team + "]  Lv " + std::to_string(p.level));
            }
        }
    }

    const unsigned FS = 13;
    const float PAD = 6.f;
    float boxW = 0.f;
    std::vector<sf::Text> texts;
    for (auto& s : lines) {
        sf::Text t;
        t.setFont(font); t.setString(s); t.setCharacterSize(FS);
        t.setFillColor(sf::Color::White);
        boxW = std::max(boxW, t.getLocalBounds().width);
        texts.push_back(std::move(t));
    }
    boxW += PAD * 2.f;
    float boxH = (float)lines.size() * (FS + 4.f) + PAD * 2.f;

    float bx = sx + 12.f;
    float by = sy - boxH * 0.5f;
    if (bx + boxW > (float)winW - 4.f) bx = sx - boxW - 12.f;
    if (by < 4.f)                       by = 4.f;
    if (by + boxH > (float)winH - 4.f) by = (float)winH - boxH - 4.f;

    sf::RectangleShape bg(sf::Vector2f(boxW, boxH));
    bg.setPosition(bx, by);
    bg.setFillColor(sf::Color(10, 14, 24, 210));
    bg.setOutlineColor(sf::Color(180, 200, 255, 160));
    bg.setOutlineThickness(1.f);
    win.draw(bg);

    float yOff = by + PAD;
    for (int i = 0; i < (int)texts.size(); ++i) {
        texts[i].setFillColor(i == 0 ? sf::Color(255, 230, 80)
                            : texts[i].getString().toAnsiString().find("Player") != std::string::npos
                              ? sf::Color(120, 220, 255)
                              : sf::Color(200, 240, 200));
        texts[i].setPosition(bx + PAD, yOff);
        win.draw(texts[i]);
        yOff += FS + 4.f;
    }
}
