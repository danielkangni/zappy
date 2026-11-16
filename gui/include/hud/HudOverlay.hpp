#pragma once

#include <SFML/Graphics.hpp>
#include <glm/glm.hpp>
#include <algorithm>
#include <string>
#include <vector>

#include "net/GuiState.hpp"

class HudOverlay {
public:
    void init(const sf::Font &font) { _font = &font; }

    void toggle() { _open = !_open; }
    void update(float dt) {
        _anim += ((_open ? 1.f : 0.f) - _anim) * std::min(1.f, dt * 10.f);
    }

    bool handleClick(sf::Vector2i mouse);

    bool handleScroll(sf::Vector2i mouse, float wheelDelta);

    int inventoryPollTarget(float dt);

    bool consumeFullscreenClick();
    bool consumeSettingsClick();

    void draw(sf::RenderWindow &win, const GuiState *state,
              const std::string &host, int port, int mapW, int mapH);

    const std::string &selectedTeam() const { return _selTeam; }
    int selectedPlayer() const { return _selPlayer; }
    void setSelectedPlayer(int pid) { _selPlayer = pid; if (pid < 0) _invPollT = 0.f; }

    static glm::vec3 teamColor3(int i);

private:

    struct Hit { sf::FloatRect rect; int kind; std::string team; int pid; };

    static sf::Color teamColorU(int i);

    const sf::Font   *_font      = nullptr;
    bool              _open      = true;
    float             _anim      = 1.f;
    std::string       _selTeam;
    int               _selPlayer = -1;
    float             _invPollT  = 0.f;
    bool              _fullscreenClicked = false;
    bool              _settingsClicked   = false;
    std::vector<Hit>  _hits;
    float             _scroll    = 0.f;
    float             _maxScroll = 0.f;
    float             _panelX    = 0.f;

    static constexpr float WIDTH = 250.f;
};
