#include "GUIException.hpp"
#include "render/Shader.hpp"
#include <GL/glew.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::string Shader::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw GUIException("Shader::readFile: cannot open " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

unsigned int Shader::compileShader(unsigned int type, const std::string& src) {
    unsigned int shader = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(shader, 1, &c, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        glDeleteShader(shader);
        throw GUIException(std::string("Shader compile error:\n") + log);
    }
    return shader;
}

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);

    unsigned int vert = compileShader(GL_VERTEX_SHADER,   vertSrc);
    unsigned int frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);

    int success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(id, 1024, nullptr, log);
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(id);
        throw GUIException(std::string("Shader link error:\n") + log);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() {
    if (id) glDeleteProgram(id);
}

void Shader::use() const {
    glUseProgram(id);
}

void Shader::setInt(const std::string& name, int v) const {
    glUniform1i(glGetUniformLocation(id, name.c_str()), v);
}

void Shader::setFloat(const std::string& name, float v) const {
    glUniform1f(glGetUniformLocation(id, name.c_str()), v);
}

void Shader::setVec2(const std::string& name, float x, float y) const {
    glUniform2f(glGetUniformLocation(id, name.c_str()), x, y);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(id, name.c_str()), x, y, z);
}

void Shader::setMat4(const std::string& name, const float* mat) const {
    glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, mat);
}

void Shader::setMat4Array(const std::string& name, int count, const float* mats) const {
    glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), count, GL_FALSE, mats);
}

void Shader::setMat3(const std::string& name, const float* mat) const {
    glUniformMatrix3fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, mat);
}

void Shader::setBool(const std::string& name, bool v) const {
    glUniform1i(glGetUniformLocation(id, name.c_str()), v ? 1 : 0);
}
