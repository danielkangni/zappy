#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cmath>

struct Frustum {
    std::array<glm::vec4, 6> planes;

    static Frustum fromVP(const glm::mat4& vp) {
        Frustum f;

        auto row = [&](int r) {
            return glm::vec4(vp[0][r], vp[1][r], vp[2][r], vp[3][r]);
        };
        glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        f.planes[0] = r3 + r0;
        f.planes[1] = r3 - r0;
        f.planes[2] = r3 + r1;
        f.planes[3] = r3 - r1;
        f.planes[4] = r3 + r2;
        f.planes[5] = r3 - r2;
        for (auto& p : f.planes) {
            float len = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
            if (len > 1e-6f) p /= len;
        }
        return f;
    }

    bool sphereVisible(const glm::vec3& c, float r) const {
        for (const auto& p : planes)
            if (p.x*c.x + p.y*c.y + p.z*c.z + p.w < -r)
                return false;
        return true;
    }
};
