#pragma once
#include <string>

class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    void use() const;
    void setInt  (const std::string& name, int         v) const;
    void setFloat(const std::string& name, float       v) const;
    void setVec2 (const std::string& name, float x, float y) const;
    void setVec3 (const std::string& name, float x, float y, float z) const;
    void setMat4     (const std::string& name, const float* mat) const;
    void setMat4Array(const std::string& name, int count, const float* mats) const;
    void setMat3     (const std::string& name, const float* mat) const;
    void setBool (const std::string& name, bool        v) const;

private:
    unsigned int id;

    static unsigned int compileShader(unsigned int type, const std::string& src);
    static std::string  readFile(const std::string& path);
};
