#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "app/Particle.hpp"

class Shader;

class Constellation {
public:
    Constellation();

    void tick(bool spaceHeld, bool adminMode, float dt);

    void updateMewEnergy(bool adminMode, float dt, const glm::vec3& mewPos, float elapsed);

    void renderNightSky(Shader& star, unsigned vao, const glm::mat4& view,
                        const glm::mat4& proj, const glm::vec3& camPos,
                        float elapsed, float nightF) const;

    void renderMewEnergy(Shader& star, unsigned vao, const glm::mat4& view,
                         const glm::mat4& proj, float elapsed) const;

private:
    struct SkyStar { glm::vec3 dir; float size, phase, twFreq, bright; };
    std::vector<SkyStar> _stars;
    glm::vec3 _dipperDir[7];
    glm::vec3 _dipperCenter{0.f};
    float _charge   = 0.f;
    bool  _active   = false;
    float _waveT    = -1.f;
    float _litT     = -1.f;
    std::vector<Spark> _motes;
    float _wavePrev = -2.f;
};
