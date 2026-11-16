#include "hud/HudOverlay.hpp"

glm::vec3 HudOverlay::teamColor3(int i)
{
    static const glm::vec3 pal[] = {
        {0.20f, 0.80f, 1.00f}, {1.00f, 0.55f, 0.15f}, {0.45f, 1.00f, 0.40f},
        {1.00f, 0.35f, 0.75f}, {1.00f, 0.85f, 0.20f}, {0.65f, 0.45f, 1.00f},
    };
    return pal[((i % 6) + 6) % 6];
}

sf::Color HudOverlay::teamColorU(int i)
{
    glm::vec3 c = teamColor3(i);
    return sf::Color((sf::Uint8)(c.r * 255), (sf::Uint8)(c.g * 255), (sf::Uint8)(c.b * 255));
}

bool HudOverlay::handleClick(sf::Vector2i mouse)
{
    for (const auto &h : _hits) {
        if (!h.rect.contains((float)mouse.x, (float)mouse.y)) continue;
        if (h.kind == 4) {
            _fullscreenClicked = true;
            return true;
        }
        if (h.kind == 5) {
            _settingsClicked = true;
            return true;
        }
        if (h.kind == 2)
            _open = !_open;
        else if (h.kind == 0) {
            _selTeam = (_selTeam == h.team) ? std::string() : h.team;
            _selPlayer = -1;
        } else if (h.kind == 1)
            _selPlayer = (_selPlayer == h.pid) ? -1 : h.pid;
        return true;
    }
    return false;
}

bool HudOverlay::consumeFullscreenClick()
{
    bool c = _fullscreenClicked;
    _fullscreenClicked = false;
    return c;
}

bool HudOverlay::consumeSettingsClick()
{
    bool c = _settingsClicked;
    _settingsClicked = false;
    return c;
}

int HudOverlay::inventoryPollTarget(float dt)
{
    _invPollT -= dt;
    if (_selPlayer >= 0 && _invPollT <= 0.f) {
        _invPollT = 0.33f;
        return _selPlayer;
    }
    return -1;
}

bool HudOverlay::handleScroll(sf::Vector2i mouse, float wheelDelta)
{
    if (_anim < 0.5f || (float)mouse.x < _panelX)
        return false;
    _scroll = std::min(std::max(_scroll - wheelDelta * 34.f, 0.f), _maxScroll);
    return true;
}

void HudOverlay::draw(sf::RenderWindow &win, const GuiState *state,
                      const std::string &host, int port, int mapW, int mapH)
{
    sf::Vector2u ws = win.getSize();
    const sf::Font &font = *_font;

    _hits.clear();

    {
        const float iconSz = 16.f, pad = 12.f, gap = 8.f;

        sf::RectangleShape fsIcon(sf::Vector2f(iconSz, iconSz));
        fsIcon.setPosition(pad, pad);
        fsIcon.setFillColor(sf::Color::Transparent);
        fsIcon.setOutlineColor(sf::Color(220, 228, 240, 210));
        fsIcon.setOutlineThickness(1.6f);
        win.draw(fsIcon);
        _hits.push_back({sf::FloatRect(pad - 4.f, pad - 4.f, iconSz + 8.f, iconSz + 8.f),
                         4, "", -1});

        float gx = pad + iconSz + gap;
        sf::RectangleShape gearOuter(sf::Vector2f(iconSz, iconSz));
        gearOuter.setPosition(gx, pad);
        gearOuter.setFillColor(sf::Color(30, 36, 50, 200));
        gearOuter.setOutlineColor(sf::Color(170, 195, 230, 210));
        gearOuter.setOutlineThickness(1.6f);
        win.draw(gearOuter);

        sf::RectangleShape gearInner(sf::Vector2f(6.f, 6.f));
        gearInner.setPosition(gx + 5.f, pad + 5.f);
        gearInner.setFillColor(sf::Color(170, 195, 230, 200));
        win.draw(gearInner);
        _hits.push_back({sf::FloatRect(gx - 4.f, pad - 4.f, iconSz + 8.f, iconSz + 8.f),
                         5, "", -1});
    }

    sf::Text info;
    info.setFont(font);
    std::string status;
    if (state) {
        status = "ONLINE " + host + ":" + std::to_string(port) +
                 "   grid " + std::to_string(state->width) + "x" + std::to_string(state->height) +
                 "   t/u " + std::to_string(state->timeUnit) +
                 "   teams " + std::to_string(state->teams.size()) +
                 "   players " + std::to_string(state->players.size()) +
                 "   eggs " + std::to_string(state->eggs.size());
        info.setFillColor(sf::Color(150, 220, 255, 200));
    } else {
        status = "OFFLINE (viewer)   map.json " +
                 std::to_string(mapW) + "x" + std::to_string(mapH);
        info.setFillColor(sf::Color(230, 180, 140, 190));
    }
    info.setString(status);
    info.setCharacterSize(18);
    sf::FloatRect b = info.getLocalBounds();
    info.setPosition(ws.x - b.width - 14.f, 14.f);
    win.draw(info);

    const float pad = 14.f;
    float panelX = ws.x - WIDTH * _anim;
    float tabW = 26.f, tabH = 60.f, tabX = panelX - tabW, tabY = 96.f;
    sf::RectangleShape tab(sf::Vector2f(tabW, tabH));
    tab.setPosition(tabX, tabY);
    tab.setFillColor(sf::Color(20, 26, 38, 210));
    tab.setOutlineColor(sf::Color(90, 130, 180, 200));
    tab.setOutlineThickness(1.f);
    win.draw(tab);
    sf::Text arrow; arrow.setFont(font);
    arrow.setString(_open ? ">" : "<");
    arrow.setCharacterSize(20); arrow.setFillColor(sf::Color(180, 210, 255));
    arrow.setPosition(tabX + 8.f, tabY + 16.f); win.draw(arrow);
    _hits.push_back({sf::FloatRect(tabX, tabY, tabW, tabH), 2, "", -1});

    if (_anim > 0.01f && state) {
        _panelX = panelX;
        sf::RectangleShape panel(sf::Vector2f(WIDTH, (float)ws.y));
        panel.setPosition(panelX, 0.f);
        panel.setFillColor(sf::Color(14, 18, 28, 235));
        win.draw(panel);

        float x = panelX + pad, y = 46.f;

        {
            sf::Text eh; eh.setFont(font); eh.setString("EVENTS");
            eh.setCharacterSize(18); eh.setFillColor(sf::Color(185, 205, 240));
            eh.setPosition(x, y); win.draw(eh); y += 26.f;
            float sr = state->secondsToReset;
            std::string ts;
            if (sr < 0.f) {
                ts = "--:--";
            } else {
                int s = (int)sr, mm = s / 60, ss = s % 60;
                ts = std::to_string(mm) + ":" + (ss < 10 ? "0" : "") + std::to_string(ss);
            }
            sf::RectangleShape er(sf::Vector2f(WIDTH - 2 * pad, 24.f));
            er.setPosition(x, y);
            bool soon = (sr >= 0.f && sr <= 12.f);
            er.setFillColor(soon ? sf::Color(255, 180, 90, 70) : sf::Color(255, 255, 255, 12));
            win.draw(er);
            sf::Text et; et.setFont(font); et.setString("Resource reset");
            et.setCharacterSize(14); et.setFillColor(sf::Color(150, 225, 170));
            et.setPosition(x + 6.f, y + 3.f); win.draw(et);
            sf::Text ev; ev.setFont(font); ev.setString(ts);
            ev.setCharacterSize(15); ev.setFillColor(sf::Color(240, 240, 248));
            sf::FloatRect vb = ev.getLocalBounds();
            ev.setPosition(x + (WIDTH - 2 * pad) - vb.width - 8.f, y + 2.f);
            win.draw(ev);
            y += 38.f;

            auto drawFeed = [&](const char *label, const char *prefix,
                                 sf::Color color, int maxLines) {
                sf::Text lbl; lbl.setFont(font); lbl.setString(label);
                lbl.setCharacterSize(13); lbl.setFillColor(sf::Color(150, 180, 215));
                lbl.setPosition(x, y); win.draw(lbl); y += 18.f;

                std::vector<const std::string *> matches;
                for (int i = (int)state->rawFeed.size() - 1;
                     i >= 0 && (int)matches.size() < maxLines; --i) {
                    if (state->rawFeed[i].rfind(prefix, 0) == 0)
                        matches.push_back(&state->rawFeed[i]);
                }
                const float lineH = 13.f;
                int shown = (int)matches.size();
                float h = shown * lineH + 8.f;
                sf::RectangleShape bg(sf::Vector2f(WIDTH - 2 * pad, h));
                bg.setPosition(x, y);
                bg.setFillColor(sf::Color(0, 0, 0, 110));
                win.draw(bg);
                for (int k = 0; k < shown; ++k) {
                    const std::string &raw = *matches[shown - 1 - k];
                    sf::Text lt; lt.setFont(font);
                    lt.setString(raw.size() > 30 ? raw.substr(0, 27) + "..." : raw);
                    lt.setCharacterSize(11);
                    lt.setFillColor(color);
                    lt.setPosition(x + 4.f, y + 4.f + k * lineH);
                    win.draw(lt);
                }
                y += h + 10.f;
            };
            drawFeed("SERVER >>", "<< ", sf::Color(140, 235, 170, 230), 5);
            drawFeed("CLIENT >>", ">> ", sf::Color(255, 200, 110, 230), 4);
            y += 4.f;
        }

        sf::Text title; title.setFont(font); title.setString("TEAMS");
        title.setCharacterSize(20); title.setFillColor(sf::Color(200, 220, 255));
        title.setPosition(x, y); win.draw(title); y += 34.f;

        const float listTop = y;
        bool popupOpen = (_selPlayer >= 0 && state->players.count(_selPlayer));
        const float popupH = popupOpen ? 234.f : 0.f;
        float listH = std::max(1.f, (float)ws.y - listTop - popupH);

        sf::View prevView = win.getView();
        sf::View lv;
        lv.setSize(WIDTH, listH);
        lv.setCenter(panelX + WIDTH * 0.5f, listTop + listH * 0.5f + _scroll);
        lv.setViewport(sf::FloatRect(panelX / ws.x, listTop / ws.y, WIDTH / ws.x, listH / ws.y));
        win.setView(lv);

        float cy = listTop;
        auto rowOnScreen = [&](float ry, float rh) {
            float sy = ry - _scroll;
            return sy + rh > listTop && sy < listTop + listH;
        };
        for (int ti = 0; ti < (int)state->teams.size(); ++ti) {
            const std::string &tname = state->teams[ti];
            sf::Color tcol = teamColorU(ti);
            bool selT = (_selTeam == tname);
            sf::RectangleShape hr(sf::Vector2f(WIDTH - 2 * pad, 26.f));
            hr.setPosition(x, cy);
            hr.setFillColor(selT ? sf::Color(tcol.r, tcol.g, tcol.b, 80)
                                 : sf::Color(255, 255, 255, 14));
            win.draw(hr);
            sf::Text th; th.setFont(font);
            th.setString((selT ? "v " : "> ") + tname);
            th.setCharacterSize(18); th.setFillColor(tcol);
            th.setPosition(x + 6.f, cy + 2.f); win.draw(th);
            if (rowOnScreen(cy, 26.f))
                _hits.push_back({sf::FloatRect(x, cy - _scroll, WIDTH - 2 * pad, 26.f),
                                 0, tname, -1});
            cy += 30.f;

            std::vector<const GuiPlayer *> tp;
            for (const auto &kv : state->players)
                if (kv.second.team == tname) tp.push_back(&kv.second);
            std::sort(tp.begin(), tp.end(),
                      [](const GuiPlayer *a, const GuiPlayer *c) { return a->id < c->id; });
            for (const GuiPlayer *p : tp) {
                bool selP = (_selPlayer == p->id);
                sf::RectangleShape pr(sf::Vector2f(WIDTH - 2 * pad - 12.f, 22.f));
                pr.setPosition(x + 12.f, cy);
                pr.setFillColor(selP ? sf::Color(tcol.r, tcol.g, tcol.b, 60)
                                     : sf::Color(255, 255, 255, 8));
                win.draw(pr);
                sf::Text pt; pt.setFont(font);
                pt.setString("Player #" + std::to_string(p->id) + "   Lv " +
                             (p->level == 0 ? std::string("egg") : std::to_string(p->level)));
                pt.setCharacterSize(15);
                pt.setFillColor(selP ? sf::Color(255, 255, 255) : sf::Color(200, 210, 225));
                pt.setPosition(x + 18.f, cy + 2.f); win.draw(pt);
                if (rowOnScreen(cy, 22.f))
                    _hits.push_back({sf::FloatRect(x + 12.f, cy - _scroll, WIDTH - 2 * pad - 12.f,
                                     22.f), 1, tname, p->id});
                cy += 24.f;
            }
            cy += 8.f;
        }
        win.setView(prevView);
        _maxScroll = std::max(0.f, (cy - listTop) - listH);
        if (_scroll > _maxScroll) _scroll = _maxScroll;

        if (popupOpen) {
            const GuiPlayer &sp = state->players.at(_selPlayer);
            float pw = WIDTH - 2 * pad;
            float py = (float)ws.y - 226.f;
            sf::RectangleShape box(sf::Vector2f(pw, 218.f));
            box.setPosition(x, py);
            box.setFillColor(sf::Color(8, 12, 20, 244));
            box.setOutlineColor(sf::Color(90, 130, 180, 170));
            box.setOutlineThickness(1.f);
            win.draw(box);
            sf::Text ht; ht.setFont(font);
            ht.setString("Player #" + std::to_string(sp.id) + "  inventory");
            ht.setCharacterSize(15); ht.setFillColor(sf::Color(210, 225, 255));
            ht.setPosition(x + 8.f, py + 6.f); win.draw(ht);
            static const char *RNAME[7] = {"Food", "Linemate", "Deraumere",
                "Sibur", "Mendiane", "Phiras", "Thystame"};
            static const sf::Color RCOL[7] = {
                sf::Color(120, 220, 120), sf::Color(205, 205, 215),
                sf::Color(120, 180, 255), sf::Color(110, 230, 210),
                sf::Color(230, 170,  90), sf::Color(220, 120, 220),
                sf::Color(255, 225, 120)};
            float ry = py + 32.f;
            for (int i = 0; i < 7; ++i) {
                sf::Text nm; nm.setFont(font); nm.setString(RNAME[i]);
                nm.setCharacterSize(14); nm.setFillColor(RCOL[i]);
                nm.setPosition(x + 12.f, ry); win.draw(nm);
                sf::Text ct; ct.setFont(font);
                ct.setString(sp.invKnown ? std::to_string(sp.inv[i]) : "...");
                ct.setCharacterSize(14); ct.setFillColor(sf::Color(235, 235, 245));
                sf::FloatRect cb = ct.getLocalBounds();
                ct.setPosition(x + pw - cb.width - 14.f, ry); win.draw(ct);
                ry += 25.f;
            }
        } else if (_selPlayer >= 0) {
            _selPlayer = -1;
        }

        _hits.push_back({sf::FloatRect(panelX, 0.f, WIDTH, (float)ws.y), 3, "", -1});
    }

    if (state) {
        auto teamCol = [&](const std::string &tn) -> sf::Color {
            for (int k = 0; k < (int)state->teams.size(); ++k)
                if (state->teams[k] == tn) return teamColorU(k);
            return sf::Color(190, 195, 205);
        };
        const int   maxLines = 13;
        const float lineH = 17.f;
        int n = (int)state->log.size();
        int shown = std::min(n, maxLines);
        float baseY = ws.y - 16.f - lineH;
        if (shown > 0) {
            sf::RectangleShape bg(sf::Vector2f(300.f, shown * lineH + 12.f));
            bg.setPosition(8.f, baseY - (shown - 1) * lineH - 6.f);
            bg.setFillColor(sf::Color(0, 0, 0, 95));
            win.draw(bg);
        }
        for (int k = 0; k < shown; ++k) {
            const GuiLogEntry &e = state->log[n - 1 - k];
            float ly = baseY - k * lineH;
            int alpha = (int)(255.f * (1.f - 0.55f * (float)k / maxLines));
            sf::Text nm; nm.setFont(font);
            nm.setString("Player #" + std::to_string(e.playerId));
            nm.setCharacterSize(14);
            sf::Color nc = teamCol(e.team); nc.a = (sf::Uint8)alpha;
            nm.setFillColor(nc);
            nm.setPosition(14.f, ly); win.draw(nm);
            sf::Color ac = e.kind == 1 ? sf::Color(150, 240, 160)
                         : e.kind == 2 ? sf::Color(245, 120, 110)
                         : e.kind == 3 ? sf::Color(190, 160, 255)
                         : e.kind == 4 ? sf::Color(110,  70, 160)
                         :               sf::Color(208, 211, 220);
            ac.a = (sf::Uint8)alpha;
            sf::Text at; at.setFont(font);
            at.setString(" " + e.action);
            at.setCharacterSize(14); at.setFillColor(ac);
            at.setPosition(14.f + nm.getLocalBounds().width + 5.f, ly);
            win.draw(at);
        }
    }
}
