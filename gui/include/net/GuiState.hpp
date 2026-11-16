#pragma once

#include <array>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

inline constexpr int GUI_RES_COUNT = 7;

struct GuiTile {
    std::array<int, GUI_RES_COUNT> res{};
};

struct GuiLogEntry {
    int         playerId = 0;
    std::string team;
    std::string action;
    int         kind = 0;
};

struct GuiPlayer {
    int         id          = 0;
    int         x           = 0;
    int         y           = 0;
    int         orientation = 1;
    int         level       = 1;
    std::string team;
    bool        incanting   = false;
    bool        ejected     = false;
    std::array<int, GUI_RES_COUNT> inv{};
    bool        invKnown    = false;
};

struct GuiEgg {
    int         id   = 0;
    int         x    = 0;
    int         y    = 0;
    std::string team;
};

struct GuiIncantation {
    int  x       = 0;
    int  y       = 0;
    bool resolved = false;
    bool success  = false;
};

struct GuiState {
    int width    = 0;
    int height   = 0;
    int timeUnit = 100;
    bool sizeKnown = false;
    float secondsToReset = -1.f;

    std::vector<std::string>          teams;
    std::vector<GuiTile>              tiles;
    std::unordered_map<int, GuiPlayer> players;
    std::unordered_map<int, GuiEgg>    eggs;

    bool resDirty = false;
    std::string winner;
    std::deque<GuiLogEntry> log;
    unsigned long logTotal = 0;

    std::deque<std::string> rawFeed;

    GuiTile &tileAt(int x, int y) { return tiles[y * width + x]; }
    const GuiTile &tileAt(int x, int y) const { return tiles[y * width + x]; }

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    void resizeGrid(int w, int h) {
        width = w;
        height = h;
        tiles.assign(static_cast<std::size_t>(w) * h, GuiTile{});
        sizeKnown = true;
    }
};
