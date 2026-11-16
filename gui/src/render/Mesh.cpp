#include "render/Mesh.hpp"
#include <GL/glew.h>
#include <cmath>
#include <stdexcept>

Mesh::Mesh(const std::vector<float>& verts, const std::vector<unsigned int>& indices)
    : indexCount((int)indices.size())
{

    const int stride = ((int)verts.size() % 9 == 0 && (int)verts.size() % 6 != 0)
                       ? 9 : 6;
    _stride = stride;

    if (!verts.empty()) {
        float sumX = 0, sumY = 0, sumZ = 0;
        int   n    = (int)verts.size() / stride;
        for (int i = 0; i < n; ++i) {
            sumX += verts[i*stride + 0];
            sumY += verts[i*stride + 1];
            sumZ += verts[i*stride + 2];
        }
        _centroid = {sumX/n, sumY/n, sumZ/n};

        float maxR2 = 0;
        for (int i = 0; i < n; ++i) {
            float dx = verts[i*stride+0] - _centroid.x;
            float dy = verts[i*stride+1] - _centroid.y;
            float dz = verts[i*stride+2] - _centroid.z;
            float r2 = dx*dx + dy*dy + dz*dz;
            if (r2 > maxR2) maxR2 = r2;
        }
        _radius = std::sqrt(maxR2);
        if (_radius < 1e-5f) _radius = 1.f;

        _yMin = verts[1];
        for (int i = 1; i < n; ++i) {
            float y = verts[i*stride + 1];
            if (y < _yMin) _yMin = y;
        }
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    GLsizei byteStride = stride * sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    if (stride == 9) {
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, byteStride, (void*)(6*sizeof(float)));
        glEnableVertexAttribArray(2);
    }

    glBindVertexArray(0);
}

Mesh::Mesh(Mesh&& other) noexcept
    : vao(other.vao), vbo(other.vbo), ebo(other.ebo),
      indexCount(other.indexCount), _stride(other._stride),
      _centroid(other._centroid), _radius(other._radius), _yMin(other._yMin)
{
    other.vao = 0; other.vbo = 0; other.ebo = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        cleanup();
        vao        = other.vao;
        vbo        = other.vbo;
        ebo        = other.ebo;
        indexCount = other.indexCount;
        _stride    = other._stride;
        _centroid  = other._centroid;
        _radius    = other._radius;
        _yMin      = other._yMin;
        other.vao = 0; other.vbo = 0; other.ebo = 0;
    }
    return *this;
}

void Mesh::cleanup() {
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
}

Mesh::~Mesh() {
    cleanup();
}

void Mesh::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

int Mesh::vertCount() const {
    return indexCount;
}

glm::vec3 Mesh::centroid() const {
    return _centroid;
}

float Mesh::boundingRadius() const { return _radius; }
float Mesh::yMin()           const { return _yMin;   }
