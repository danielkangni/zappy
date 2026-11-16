#include "app/OutroScene.hpp"
#include <algorithm>
#include <cmath>

static float easeOutCubic(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    float u = 1.f - t;
    return 1.f - u * u * u;
}

void OutroScene::start(const std::string& winnerTeam)
{
    _active = true;
    _t      = 0.f;
    _team   = winnerTeam;
}

void OutroScene::update(float dt)
{
    if (!_active) return;
    _t += dt;
}

void OutroScene::draw(sf::RenderWindow& window,
                      const sf::Font& endFont, const sf::Font& titleFont)
{
    if (!_active) return;
    sf::Vector2u ws = window.getSize();
    float W = (float)ws.x, H = (float)ws.y;

    window.clear(sf::Color::White);
    window.pushGLStates();

    float barT     = easeOutCubic(std::clamp((_t - 0.25f) / 1.1f, 0.f, 1.f));
    float barW     = W * 0.40f;
    float barRestX = W * 0.60f;
    float barX     = W + (barRestX - W) * barT;

    sf::RectangleShape bar(sf::Vector2f(barW + 4.f, H));
    bar.setPosition(barX, 0.f);
    bar.setFillColor(sf::Color(10, 14, 22, 255));
    bar.setOutlineColor(sf::Color(255, 255, 255, 200));
    bar.setOutlineThickness(-2.f);
    window.draw(bar);

    if (barT > 0.05f) {
        sf::Text end;
        end.setFont(endFont);
        end.setString("END");
        unsigned sz = (unsigned)(H * 0.22f);
        end.setCharacterSize(sz);
        sf::Uint8 a = (sf::Uint8)(255.f * std::min(1.f, barT * 1.6f));
        end.setFillColor(sf::Color(255, 255, 255, a));
        end.setOutlineColor(sf::Color(0, 0, 0, a));
        end.setOutlineThickness(4.f);
        sf::FloatRect b = end.getLocalBounds();
        end.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
        end.setPosition(barX + (barW + 4.f) / 2.f, H * 0.5f);
        window.draw(end);
    }

    float leftW = barRestX;
    float textT = easeOutCubic(std::clamp((_t - 0.5f) / 1.0f, 0.f, 1.f));
    if (textT > 0.02f) {
        sf::Text title;
        title.setFont(titleFont);
        title.setString(_team + " WON");
        title.setCharacterSize((unsigned)(H * 0.09f));
        sf::Uint8 a = (sf::Uint8)(255.f * textT);
        title.setFillColor(sf::Color(0, 0, 0, a));
        title.setOutlineColor(sf::Color(255, 255, 255, a));
        title.setOutlineThickness(3.f);
        sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin(tb.left + tb.width / 2.f, 0.f);
        title.setPosition(leftW / 2.f, H * 0.25f);
        window.draw(title);
    }

    float creditT = easeOutCubic(std::clamp((_t - 1.2f) / 1.2f, 0.f, 1.f));
    if (creditT > 0.02f) {
        sf::Text credit;
        credit.setFont(titleFont);
        credit.setString("By Espoir BIAOU");
        credit.setStyle(sf::Text::Italic);
        credit.setCharacterSize((unsigned)(H * 0.052f));
        sf::Uint8 a = (sf::Uint8)(255.f * creditT);
        credit.setFillColor(sf::Color(30, 30, 30, a));
        sf::FloatRect cb = credit.getLocalBounds();
        credit.setOrigin(cb.left + cb.width / 2.f, 0.f);
        credit.setPosition(leftW / 2.f, H * 0.50f);
        window.draw(credit);
    }

    float overlayA = 1.f - easeOutCubic(_t / 1.0f);
    if (overlayA > 0.003f) {
        sf::RectangleShape ov(sf::Vector2f(W, H));
        ov.setFillColor(sf::Color(0, 0, 0, (sf::Uint8)(255.f * overlayA)));
        window.draw(ov);
    }

    window.popGLStates();
}
