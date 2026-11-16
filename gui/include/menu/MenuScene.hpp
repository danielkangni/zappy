#pragma once

#include <SFML/Graphics.hpp>
#include <string>

struct MenuConfig {
    std::string host = "127.0.0.1";
    int         port = 4242;
};

bool runMenu(sf::RenderWindow &window, sf::Font &font, sf::Font &uiFont,
             MenuConfig &out);

void drawLoadingScreen(sf::RenderWindow &window, sf::Font &font,
                       float progress, const std::string &status);

void advanceLoadingTo(sf::RenderWindow &window, sf::Font &font,
                      float target, const std::string &status,
                      float duration = 0.4f);
