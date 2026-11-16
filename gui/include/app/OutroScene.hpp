#pragma once
#include <SFML/Graphics.hpp>
#include <string>

class OutroScene {
public:
    void start(const std::string& winnerTeam);
    bool active() const { return _active; }
    void update(float dt);
    void draw(sf::RenderWindow& window,
              const sf::Font& endFont, const sf::Font& titleFont);

private:
    bool        _active = false;
    float       _t      = 0.f;
    std::string _team;
};
