#pragma once
#include <glm/glm.hpp>
#include <SFML/Graphics.hpp>

struct MapData;
struct GuiState;

void drawTileGridGL(const glm::mat4& view, const glm::mat4& proj,
                    const MapData& map, const GuiState& gs, float cx, float cz);

void drawTileGridHUD(const glm::mat4& view, const glm::mat4& proj,
                     const MapData& map, const GuiState& gs,
                     float cx, float cz,
                     sf::RenderWindow& win, const sf::Font& font,
                     unsigned winW, unsigned winH,
                     int mouseX, int mouseY);
