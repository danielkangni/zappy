#include "GUIException.hpp"
#include "render/TexturedMesh.hpp"
#include "AssetCache.hpp"

#include <GL/glew.h>
#include <SFML/Graphics/Image.hpp>

#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <unordered_map>
#include <stdexcept>
#include <cmath>

namespace {

struct Key {
    int v, vt, vn;
    bool operator==(const Key &o) const { return v == o.v && vt == o.vt && vn == o.vn; }
};
struct KeyHash {
    std::size_t operator()(const Key &k) const {
        return (std::hash<int>()(k.v) * 73856093) ^
               (std::hash<int>()(k.vt) * 19349663) ^
               (std::hash<int>()(k.vn) * 83492791);
    }
};

Key parseCorner(const std::string &s)
{
    Key k{0, 0, 0};
    int field = 0;
    std::string cur;
    auto flush = [&]() {
        int val = cur.empty() ? 0 : std::atoi(cur.c_str());
        if (field == 0) k.v = val;
        else if (field == 1) k.vt = val;
        else k.vn = val;
        cur.clear();
    };
    for (char c : s) {
        if (c == '/') { flush(); ++field; }
        else cur += c;
    }
    flush();
    return k;
}

}

TexturedMesh::~TexturedMesh() { cleanup(); }

void TexturedMesh::cleanup()
{
    if (_ebo) glDeleteBuffers(1, &_ebo);
    if (_vbo) glDeleteBuffers(1, &_vbo);
    if (_vao) glDeleteVertexArrays(1, &_vao);
    if (_texId) glDeleteTextures(1, &_texId);
    _vao = _vbo = _ebo = _texId = 0;
}

TexturedMesh::TexturedMesh(TexturedMesh &&o) noexcept
{
    *this = std::move(o);
}

TexturedMesh &TexturedMesh::operator=(TexturedMesh &&o) noexcept
{
    if (this != &o) {
        cleanup();
        _vao = o._vao; _vbo = o._vbo; _ebo = o._ebo; _texId = o._texId;
        _indexCount = o._indexCount; _centroid = o._centroid; _radius = o._radius;
        o._vao = o._vbo = o._ebo = o._texId = 0;
        o._indexCount = 0;
    }
    return *this;
}

TexturedMesh TexturedMesh::load(const std::string &objPath, const std::string &texPath)
{
    std::string objSrc = zappy::assetBytes(objPath);
    if (objSrc.empty())
        throw GUIException("TexturedMesh: cannot open " + objPath);
    std::istringstream f(objSrc);

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 2>> uvs;
    std::vector<std::array<float, 3>> normals;

    std::vector<float>        verts;
    std::vector<unsigned int> indices;
    std::unordered_map<Key, unsigned int, KeyHash> seen;

    auto resolve = [&](int idx, int n) { return idx < 0 ? n + idx : idx - 1; };

    std::string line;
    while (std::getline(f, line)) {
        if (line.size() < 2) continue;
        std::istringstream iss(line);
        std::string tok;
        iss >> tok;
        if (tok == "v") {
            std::array<float, 3> p; iss >> p[0] >> p[1] >> p[2];
            positions.push_back(p);
        } else if (tok == "vt") {
            std::array<float, 2> t{0, 0}; iss >> t[0] >> t[1];
            uvs.push_back(t);
        } else if (tok == "vn") {
            std::array<float, 3> n; iss >> n[0] >> n[1] >> n[2];
            normals.push_back(n);
        } else if (tok == "f") {
            std::vector<Key> corners;
            std::string c;
            while (iss >> c) corners.push_back(parseCorner(c));

            for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
                for (Key k : {corners[0], corners[i], corners[i + 1]}) {
                    auto it = seen.find(k);
                    if (it != seen.end()) { indices.push_back(it->second); continue; }
                    int vi = resolve(k.v, positions.size());
                    int ti = k.vt ? resolve(k.vt, uvs.size()) : -1;
                    int ni = k.vn ? resolve(k.vn, normals.size()) : -1;
                    auto &p = positions[vi];
                    verts.insert(verts.end(), {p[0], p[1], p[2]});
                    if (ni >= 0 && ni < (int)normals.size())
                        verts.insert(verts.end(), {normals[ni][0], normals[ni][1], normals[ni][2]});
                    else
                        verts.insert(verts.end(), {0.f, 1.f, 0.f});
                    if (ti >= 0 && ti < (int)uvs.size())
                        verts.insert(verts.end(), {uvs[ti][0], uvs[ti][1]});
                    else
                        verts.insert(verts.end(), {0.f, 0.f});
                    unsigned int newIdx = (unsigned int)(seen.size());
                    seen[k] = newIdx;
                    indices.push_back(newIdx);
                }
            }
        }
    }
    if (verts.empty())
        throw GUIException("TexturedMesh: no geometry in " + objPath);

    TexturedMesh m;
    m._indexCount = (int)indices.size();

    glm::vec3 lo(1e9f), hi(-1e9f), sum(0.f);
    for (auto &p : positions) {
        glm::vec3 v(p[0], p[1], p[2]);
        lo = glm::min(lo, v); hi = glm::max(hi, v); sum += v;
    }
    m._centroid = positions.empty() ? glm::vec3(0.f) : sum / float(positions.size());
    m._radius   = glm::length(hi - lo) * 0.5f;
    if (m._radius < 1e-4f) m._radius = 1.f;

    glGenVertexArrays(1, &m._vao);
    glGenBuffers(1, &m._vbo);
    glGenBuffers(1, &m._ebo);
    glBindVertexArray(m._vao);
    glBindBuffer(GL_ARRAY_BUFFER, m._vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m._ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);
    const int stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    sf::Image img;
    std::string texBytes = zappy::assetBytes(texPath);
    if (!texBytes.empty() && img.loadFromMemory(texBytes.data(), texBytes.size())) {
        img.flipVertically();
        glGenTextures(1, &m._texId);
        glBindTexture(GL_TEXTURE_2D, m._texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.getSize().x, img.getSize().y,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return m;
}

void TexturedMesh::draw() const
{
    if (_texId) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _texId);
    }
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, _indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
