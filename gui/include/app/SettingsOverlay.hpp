#pragma once
#include <SFML/Graphics.hpp>
#include "audio/AudioManager.hpp"

class SettingsOverlay {
public:
    void init(const sf::Font& font);
    void toggle() { _open = !_open; }
    bool isOpen() const { return _open; }

    bool handlePress(sf::Vector2i mouse, AudioManager& audio);
    void handleRelease();
    void handleMove(sf::Vector2i mouse, AudioManager& audio);
    bool isDragging() const { return _drag >= 0; }

    void draw(sf::RenderWindow& win, unsigned winW, unsigned winH);

    bool handleScroll(sf::Vector2i mouse, float delta);

    float ambientMult() const { return _amb; }

private:
    static constexpr float PW = 500.f, PH = 390.f;
    static constexpr float TAB_H = 36.f, CONTENT_Y = 80.f;

    void _drawSlider(sf::RenderWindow& win, int idx,
                     float x, float y, float w,
                     const char* label, float val, float minV, float maxV,
                     const char* fmt);
    bool _hitSlider(sf::Vector2i mouse, int idx);
    void _applySlider(int idx, float mx, AudioManager& audio);

    const sf::Font* _font = nullptr;
    bool  _open     = false;
    int   _tab      = 0;
    int   _drag     = -1;

    sf::FloatRect _tracks[3];

    float _amb      = 1.0f;
    float _musicVol = 35.f;
    float _sfxVol   = 80.f;

    float _px = 0.f, _py = 0.f;
    sf::FloatRect _closeBtn;
    sf::FloatRect _tabRects[3];
    float _helperScroll    = 0.f;
    float _helperContentH  = 0.f;
};
