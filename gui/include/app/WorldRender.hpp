#pragma once
#include <glm/glm.hpp>
#include <memory>

class Shader;
class Mesh;
class TexturedMesh;
struct MapData;

void drawGround(Shader& toon, Shader& waterShader, unsigned int waterTex,
                const glm::mat4& view, const glm::mat4& proj, float elapsed,
                const glm::vec2& sunXZ, Mesh& land, Mesh& ocean, Mesh& water,
                const MapData& map, float cx, float cz, int teamCount);

void drawProps(Shader& toon, Shader& outline, const MapData& map,
               const glm::mat4& view, const glm::mat4& proj,
               float elapsed, float ambScale, float treeGlow,
               float cx, float cz, int teamCount);

void drawResources(Shader& egg, const std::unique_ptr<TexturedMesh>* gemTex,
                   const MapData& map, const glm::vec3& sunDir,
                   const glm::vec3& lightCol, float ambScale,
                   const glm::mat4& view, const glm::mat4& proj, float elapsed,
                   float cx, float cz, int teamCount);

void drawFood(Shader& egg, const std::unique_ptr<TexturedMesh>* foodMesh,
              const MapData& map, const glm::vec3& sunDir,
              const glm::vec3& lightCol, float ambScale,
              const glm::mat4& view, const glm::mat4& proj, float elapsed,
              float cx, float cz, int teamCount);

void drawClouds(Shader& toon, const MapData& map, float elapsed);
