/*
** EPITECH PROJECT, 2026
** G-YEP-400-COT-4-1-zappy-5
** File description:
** Player
*/

#pragma once 
#include "Tile.hpp"
#include <array>
#include <string>
#include <deque>
 
enum Orientation {
    NORTH = 1,
    EAST  = 2,
    SOUTH = 3,
    WEST  = 4
};

struct Action {
    std::string command;      // Raw command
    std::string name;         // Parsed command name
    std::string arg;          // Parsed argument
    int remainingTicks;
    bool isStarted;
};
 
struct Player {
    int         id;
    int         fd;
    int         x;
    int         y;
    Orientation orientation;
    int         level;
    bool        isDead;
    bool        isIncanting;
    std::array<int, RESOURCE_COUNT> inventory{};
    std::string team;
 
    std::deque<Action> actionQueue;
    int foodTimer;
 
    Player(int id, int fd, int x, int y, const std::string &team);
 
    void turnRight();
    void turnLeft();
};
