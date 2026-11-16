#pragma once
#include <string>
#include "Mesh.hpp"

class ObjLoader {
public:
    static Mesh load(const std::string& path);
};
