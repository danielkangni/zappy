/*
** EPITECH PROJECT, 2026
** G-YEP-400-COT-4-1-zappy-5
** File description:
** Tile
*/

#pragma once
#include <array>
#include <vector>
 
enum Resource {
    FOOD      = 0,
    LINEMATE  = 1,
    DERAUMERE = 2,
    SIBUR     = 3,
    MENDIANE  = 4,
    PHIRAS    = 5,
    THYSTAME  = 6,
    RESOURCE_COUNT = 7
};
 
struct Tile {
    std::array<int, RESOURCE_COUNT> resources{};
 
    int  get(Resource r) const {
        return resources[r];
    }
    void add(Resource r, int n = 1) {
        resources[r] += n;
    }
    bool remove(Resource r) {
        if (resources[r] > 0) {
            resources[r] -= 1;
            return true;
        }
        return false;
    }
};
