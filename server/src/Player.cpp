/*
** EPITECH PROJECT, 2026
** G-YEP-400-COT-4-1-zappy-5
** File description:
** Player
*/

#include "../include/Player.hpp"
 
Player::Player(int id, int fd, int x, int y, const std::string &team) : id(id), fd(fd), x(x), y(y),
    orientation(NORTH), level(1), isDead(false), isIncanting(false), team(team), foodTimer(126)
{
    inventory.fill(0);
    inventory[FOOD] = 10;
}
 
void Player::turnRight()
{
    orientation = static_cast<Orientation>((orientation % 4) + 1);
}
 
void Player::turnLeft()
{
    orientation = static_cast<Orientation>(((orientation - 2 + 4) % 4) + 1);
}
