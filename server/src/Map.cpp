/*
** EPITECH PROJECT, 2026
** G-YEP-400-COT-4-1-zappy-5
** File description:
** Map
*/

#include "../include/Map.hpp"
#include <cstdlib>
#include <stdexcept>
 
static const double DENSITIES[RESOURCE_COUNT] = {
    0.5,   // FOOD
    0.3,   // LINEMATE
    0.15,  // DERAUMERE
    0.1,   // SIBUR
    0.1,   // MENDIANE
    0.08,  // PHIRAS
    0.05,  // THYSTAME
};
 
Map::Map(int width, int height) : _width(width), _height(height), _tiles(height, std::vector<Tile>(width))
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Map dimensions must be > 0");
    spawnResources();
}
 
int Map::wrapX(int x) const
{
    return ((x % _width) + _width) % _width;
}
 
int Map::wrapY(int y) const
{
    return ((y % _height) + _height) % _height;
}
 
Tile &Map::getTile(int x, int y)
{
    return _tiles[wrapY(y)][wrapX(x)];
}
 
const Tile &Map::getTile(int x, int y) const
{
    return _tiles[wrapY(y)][wrapX(x)];
}
 
void Map::_spawnResource(Resource r, double density)
{
    int total = static_cast<int>(_width * _height * density);
    int current = 0;
    for (int y = 0; y < _height; y++)
        for (int x = 0; x < _width; x++)
            current += _tiles[y][x].get(r);

    int toSpawn = total - current;
    for (int i = 0; i < toSpawn; i++) {
        int rx = std::rand() % _width;
        int ry = std::rand() % _height;
        _tiles[ry][rx].add(r);
    }
}
 
void Map::spawnResources()
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
        _spawnResource(static_cast<Resource>(r), DENSITIES[r]);
}