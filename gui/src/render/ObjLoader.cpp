#include "GUIException.hpp"
#include "render/ObjLoader.hpp"
#include "AssetCache.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <array>
#include <tuple>
#include <unordered_map>
#include <stdexcept>
#include <cmath>

struct VertKey {
    int pi, ni;
    bool operator==(const VertKey& o) const { return pi==o.pi && ni==o.ni; }
};
struct VertKeyHash {
    std::size_t operator()(const VertKey& k) const {
        return std::hash<long long>()(((long long)k.pi << 32) | (unsigned)k.ni);
    }
};

Mesh ObjLoader::load(const std::string& path) {
    std::string src = zappy::assetBytes(path);
    if (src.empty())
        throw GUIException("ObjLoader: cannot open " + path);
    std::istringstream file(src);

    std::vector<std::array<float,3>> positions;
    std::vector<std::array<float,3>> normals;
    std::vector<std::array<float,3>> vertColors;

    struct FaceCorner { int pi, ni; };
    std::vector<std::array<FaceCorner,3>> faces;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v") {
            float x, y, z; ss >> x >> y >> z;
            positions.push_back({x, y, z});
            float r, g, b;
            if (ss >> r >> g >> b) vertColors.push_back({r, g, b});
            else                   vertColors.push_back({1.f, 1.f, 1.f});
        } else if (token == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            normals.push_back({x, y, z});
        } else if (token == "f") {

            std::vector<FaceCorner> polygon;
            std::string cornerStr;
            while (ss >> cornerStr) {
                FaceCorner fc{-1, -1};

                auto p1 = cornerStr.find('/');
                if (p1 == std::string::npos) {
                    fc.pi = std::stoi(cornerStr) - 1;
                } else {
                    fc.pi = std::stoi(cornerStr.substr(0, p1)) - 1;
                    auto p2 = cornerStr.find('/', p1+1);
                    if (p2 == std::string::npos) {

                    } else {
                        std::string nStr = cornerStr.substr(p2+1);
                        if (!nStr.empty()) {
                            fc.ni = std::stoi(nStr) - 1;
                        }
                    }
                }
                polygon.push_back(fc);
            }

            for (size_t i = 1; i + 1 < polygon.size(); ++i) {
                faces.push_back({polygon[0], polygon[i], polygon[i+1]});
            }
        }
    }

    bool computeNormals = normals.empty();
    if (computeNormals) {

        normals.resize(faces.size());
        for (size_t fi = 0; fi < faces.size(); ++fi) {
            auto& f = faces[fi];
            auto& p0 = positions[f[0].pi];
            auto& p1 = positions[f[1].pi];
            auto& p2 = positions[f[2].pi];
            float ax = p1[0]-p0[0], ay = p1[1]-p0[1], az = p1[2]-p0[2];
            float bx = p2[0]-p0[0], by = p2[1]-p0[1], bz = p2[2]-p0[2];
            float nx = ay*bz - az*by;
            float ny = az*bx - ax*bz;
            float nz = ax*by - ay*bx;
            float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (len > 1e-8f) { nx/=len; ny/=len; nz/=len; }
            normals[fi] = {nx, ny, nz};
            for (int k = 0; k < 3; ++k) f[k].ni = (int)fi;
        }
    }

    bool hasVCol = false;
    for (auto& c : vertColors)
        if (c[0] < 0.999f || c[1] < 0.999f || c[2] < 0.999f) { hasVCol = true; break; }

    const int stride = hasVCol ? 9 : 6;
    std::vector<float>        verts;
    std::vector<unsigned int> indices;
    std::unordered_map<VertKey, unsigned int, VertKeyHash> cache;

    for (auto& face : faces) {
        for (int k = 0; k < 3; ++k) {
            VertKey key{face[k].pi, face[k].ni};
            auto it = cache.find(key);
            if (it != cache.end()) {
                indices.push_back(it->second);
            } else {
                unsigned int idx = (unsigned int)(verts.size() / stride);
                cache[key] = idx;
                auto& p = positions[face[k].pi];
                verts.push_back(p[0]); verts.push_back(p[1]); verts.push_back(p[2]);
                if (face[k].ni >= 0 && face[k].ni < (int)normals.size()) {
                    auto& n = normals[face[k].ni];
                    verts.push_back(n[0]); verts.push_back(n[1]); verts.push_back(n[2]);
                } else {
                    verts.push_back(0); verts.push_back(1); verts.push_back(0);
                }
                if (hasVCol) {
                    auto& c = vertColors[face[k].pi];
                    verts.push_back(c[0]); verts.push_back(c[1]); verts.push_back(c[2]);
                }
                indices.push_back(idx);
            }
        }
    }

    if (verts.empty()) {

        verts   = {0,0,0, 0,1,0, 1,0,0, 0,1,0, 0,0,1, 0,1,0};
        indices = {0,1,2};
    }

    return Mesh(verts, indices);
}
