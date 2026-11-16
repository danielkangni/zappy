#include "app/SettingsOverlay.hpp"
#include <cstdio>
#include <cmath>

static sf::Color BG   (14,  18,  28,  245);
static sf::Color PANEL(20,  26,  40,  240);
static sf::Color ACCENT(120, 175, 255, 220);
static sf::Color DIM  (160, 175, 200, 180);
static sf::Color WHITE(235, 240, 250, 255);

void SettingsOverlay::init(const sf::Font& font) { _font = &font; }

static void box(sf::RenderWindow& w, float x, float y, float bw, float bh,
                sf::Color fill, sf::Color outline = sf::Color::Transparent, float thick = 0.f)
{
    sf::RectangleShape r(sf::Vector2f(bw, bh));
    r.setPosition(x, y);
    r.setFillColor(fill);
    if (thick > 0.f) { r.setOutlineColor(outline); r.setOutlineThickness(thick); }
    w.draw(r);
}

static void label(sf::RenderWindow& w, const sf::Font& f, const std::string& s,
                  float x, float y, unsigned sz, sf::Color col)
{
    sf::Text t; t.setFont(f); t.setString(s);
    t.setCharacterSize(sz); t.setFillColor(col);
    t.setPosition(x, y); w.draw(t);
}

void SettingsOverlay::_drawSlider(sf::RenderWindow& win, int idx,
                                  float x, float y, float w,
                                  const char* lbl, float val, float minV, float maxV,
                                  const char* fmt)
{
    label(win, *_font, lbl, x, y, 15, DIM);
    y += 22.f;
    const float TH = 4.f, KW = 14.f, KH = 22.f;
    float norm = (val - minV) / (maxV - minV);
    float kx   = x + norm * (w - KW);

    box(win, x, y + KH/2.f - TH/2.f, w, TH, sf::Color(50, 60, 80, 220));
    box(win, x, y + KH/2.f - TH/2.f, kx - x + KW/2.f, TH, ACCENT);
    box(win, kx, y, KW, KH, WHITE, ACCENT, 1.5f);

    char buf[32]; std::snprintf(buf, sizeof(buf), fmt, val);
    label(win, *_font, buf, x + w + 12.f, y + 2.f, 14, WHITE);

    _tracks[idx] = sf::FloatRect(x, y, w, KH);
}

bool SettingsOverlay::_hitSlider(sf::Vector2i mouse, int idx)
{
    return _tracks[idx].contains((float)mouse.x, (float)mouse.y);
}

void SettingsOverlay::_applySlider(int idx, float mx, AudioManager& audio)
{
    float t = (mx - _tracks[idx].left) / _tracks[idx].width;
    t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    if (idx == 0) { _amb      = 0.2f + t * 2.8f; }
    if (idx == 1) { _musicVol = t * 100.f; audio.setMusicVolume(_musicVol); }
    if (idx == 2) { _sfxVol   = t * 100.f; audio.setSfxVolume(_sfxVol); }
}

bool SettingsOverlay::handlePress(sf::Vector2i mouse, AudioManager& audio)
{
    if (!_open) return false;

    if (_closeBtn.contains((float)mouse.x, (float)mouse.y)) { _open = false; return true; }

    for (int i = 0; i < 3; ++i)
        if (_tabRects[i].contains((float)mouse.x, (float)mouse.y)) { _tab = i; return true; }

    if (_tab == 0 && _hitSlider(mouse, 0)) { _drag = 0; _applySlider(0, (float)mouse.x, audio); return true; }
    if (_tab == 1 && _hitSlider(mouse, 1)) { _drag = 1; _applySlider(1, (float)mouse.x, audio); return true; }
    if (_tab == 1 && _hitSlider(mouse, 2)) { _drag = 2; _applySlider(2, (float)mouse.x, audio); return true; }

    sf::FloatRect panelRect(_px, _py, PW, PH);
    return panelRect.contains((float)mouse.x, (float)mouse.y);
}

void SettingsOverlay::handleRelease() { _drag = -1; }

void SettingsOverlay::handleMove(sf::Vector2i mouse, AudioManager& audio)
{
    if (_drag >= 0) _applySlider(_drag, (float)mouse.x, audio);
}

void SettingsOverlay::draw(sf::RenderWindow& win, unsigned winW, unsigned winH)
{
    if (!_open) return;

    _px = (float)winW / 2.f - PW / 2.f;
    _py = (float)winH / 2.f - PH / 2.f;

    sf::RectangleShape dim(sf::Vector2f((float)winW, (float)winH));
    dim.setFillColor(sf::Color(0, 0, 0, 110));
    win.draw(dim);

    box(win, _px, _py, PW, PH, PANEL, ACCENT, 2.f);

    label(win, *_font, "SETTINGS", _px + 20.f, _py + 14.f, 20, WHITE);

    float cx = _px + PW - 36.f, cy = _py + 12.f;
    _closeBtn = sf::FloatRect(cx, cy, 24.f, 24.f);
    box(win, cx, cy, 24.f, 24.f, sf::Color(60, 30, 30, 220), sf::Color(200, 100, 100, 200), 1.5f);
    label(win, *_font, "X", cx + 6.f, cy + 2.f, 16, sf::Color(230, 130, 130));

    box(win, _px, _py + 46.f, PW, 1.f, sf::Color(60, 80, 110, 180));

    const char* tabNames[3] = { "LIGHT", "SOUND", "HELPER" };
    float tw = PW / 3.f;
    for (int i = 0; i < 3; ++i) {
        float tx = _px + i * tw;
        bool sel = (_tab == i);
        _tabRects[i] = sf::FloatRect(tx, _py + 47.f, tw, TAB_H);
        box(win, tx, _py + 47.f, tw, TAB_H,
            sel ? sf::Color(40, 60, 100, 230) : sf::Color(20, 28, 44, 200));
        sf::Text tt; tt.setFont(*_font); tt.setString(tabNames[i]);
        tt.setCharacterSize(15);
        tt.setFillColor(sel ? WHITE : DIM);
        sf::FloatRect tb = tt.getLocalBounds();
        tt.setPosition(tx + tw/2.f - tb.width/2.f - tb.left, _py + 52.f);
        win.draw(tt);
    }
    box(win, _px, _py + 47.f + TAB_H, PW, 1.f, sf::Color(60, 80, 110, 180));

    float cx0 = _px + 28.f, cw = PW - 80.f, cy0 = _py + CONTENT_Y + 16.f;

    if (_tab == 0) {
        label(win, *_font, "Ambient light brightness", cx0, cy0 - 4.f, 13, DIM);
        cy0 += 18.f;
        _drawSlider(win, 0, cx0, cy0, cw, "Brightness", _amb, 0.2f, 3.0f, "%.2fx");
    }
    else if (_tab == 1) {
        _drawSlider(win, 1, cx0, cy0,        cw, "Music volume", _musicVol, 0.f, 100.f, "%.0f%%");
        _drawSlider(win, 2, cx0, cy0 + 70.f, cw, "SFX volume",   _sfxVol,   0.f, 100.f, "%.0f%%");
    }
    else {
        const char* lines[] = {
            "CAMERA",
            "  Drag (left button)   Orbit the camera",
            "  Scroll wheel         Zoom in / out",
            "  R                    Reset view",
            "",
            "SELECTION",
            "  Click player name (sidebar)   Select player",
            "  SEE button (bottom center)    Follow in POV",
            "  Escape                        Deselect / exit POV",
            "",
            "DISPLAY",
            "  G          Toggle tile grid + resource tooltip",
            "  N          Cycle day / night / auto",
            "  F11        Toggle fullscreen",
            "  Tab        Show / hide sidebar",
            "  Gear icon  Open / close this settings panel",
            "",
            "BEAM",
            "  Selected player gets a light ray from above",
            "  that tracks their movement in real time",
            "",
            "SECRETS",
            "  A  (type password)   Unlock special mode",
        };
        const float viewTop = _py + CONTENT_Y;
        const float viewH   = PH - CONTENT_Y - 8.f;
        sf::View prevView = win.getView();
        sf::Vector2u ws   = win.getSize();
        sf::View clip;
        clip.setSize(PW, viewH);
        clip.setCenter(_px + PW * 0.5f, viewTop + viewH * 0.5f + _helperScroll);
        clip.setViewport(sf::FloatRect(_px / ws.x, viewTop / ws.y,
                                       PW / ws.x, viewH / ws.y));
        win.setView(clip);

        float ly = viewTop - 10.f;
        float startLy = ly;
        for (const char* l : lines) {
            bool isHeader = (l[0] != ' ' && l[0] != '\0');
            label(win, *_font, l, cx0 + (isHeader ? 0.f : 6.f), ly, 13,
                  isHeader ? sf::Color(150, 200, 255, 230) : DIM);
            ly += isHeader ? 20.f : 16.f;
        }
        _helperContentH = ly - startLy;

        win.setView(prevView);

        float maxScroll = std::max(0.f, _helperContentH - viewH);
        if (maxScroll > 0.f) {
            float sbx = _px + PW - 10.f, sby = viewTop;
            float sbH = viewH;
            box(win, sbx, sby, 4.f, sbH, sf::Color(40, 50, 70, 180));
            float knobH = sbH * viewH / _helperContentH;
            float knobY = sby + (_helperScroll / maxScroll) * (sbH - knobH);
            box(win, sbx, knobY, 4.f, knobH, ACCENT);
        }
    }
}

bool SettingsOverlay::handleScroll(sf::Vector2i mouse, float delta)
{
    if (!_open) return false;
    sf::FloatRect panelRect(_px, _py, PW, PH);
    if (!panelRect.contains((float)mouse.x, (float)mouse.y)) return false;
    if (_tab == 2) {
        float viewH   = PH - CONTENT_Y - 8.f;
        float maxScroll = std::max(0.f, _helperContentH - viewH);
        _helperScroll = std::max(0.f, std::min(_helperScroll - delta * 22.f, maxScroll));
    }
    return true;
}
