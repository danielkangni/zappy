#pragma once

#include <string>
#include <glm/glm.hpp>

class TexturedMesh {
public:
    TexturedMesh() = default;
    ~TexturedMesh();

    TexturedMesh(const TexturedMesh &) = delete;
    TexturedMesh &operator=(const TexturedMesh &) = delete;
    TexturedMesh(TexturedMesh &&other) noexcept;
    TexturedMesh &operator=(TexturedMesh &&other) noexcept;

    static TexturedMesh load(const std::string &objPath, const std::string &texPath);

    void draw() const;

    glm::vec3 centroid()       const { return _centroid; }
    float     boundingRadius() const { return _radius; }
    bool      hasTexture()     const { return _texId != 0; }

private:
    unsigned int _vao = 0, _vbo = 0, _ebo = 0, _texId = 0;
    int          _indexCount = 0;
    glm::vec3    _centroid{0.f};
    float        _radius = 1.f;

    void cleanup();
};
