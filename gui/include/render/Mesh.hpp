#pragma once
#include <vector>
#include <glm/glm.hpp>

class Mesh {
public:

    Mesh(const std::vector<float>& verts, const std::vector<unsigned int>& indices);
    ~Mesh();

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;
    int  vertCount() const;

    glm::vec3 centroid()       const;
    float     boundingRadius() const;
    float     yMin()           const;
    bool      hasVertexColor() const { return _stride == 9; }

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    int          indexCount = 0;

    int       _stride  = 6;
    glm::vec3 _centroid{0.f, 0.f, 0.f};
    float     _radius = 1.f;
    float     _yMin   = 0.f;

    void cleanup();
};
