/*
** EPITECH PROJECT, 2026
** G-YEP-400-COT-4-1-zappy-5
** File description:
** World
*/

#include "World.hpp"
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <algorithm>

static const int ELEVATION[7][7] = {
    {1, 1, 0, 0, 0, 0, 0}, // 1->2
    {2, 1, 1, 1, 0, 0, 0}, // 2->3
    {2, 2, 0, 1, 0, 2, 0}, // 3->4
    {4, 1, 1, 2, 0, 1, 0}, // 4->5
    {4, 1, 2, 1, 3, 0, 0}, // 5->6
    {6, 1, 2, 3, 0, 1, 0}, // 6->7
    {6, 2, 2, 2, 2, 2, 1}, // 7->8
};

static const char *RESOURCE_NAMES[RESOURCE_COUNT] = {
    "food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"
};

static int resourceFromName(const std::string &name)
{
    for (int i = 0; i < RESOURCE_COUNT; i++)
        if (name == RESOURCE_NAMES[i])
            return i;
    return -1;
}

static std::string bctMsg(int x, int y, const Tile &t)
{
    std::ostringstream oss;
    oss << "bct " << x << " " << y;
    for (int i = 0; i < RESOURCE_COUNT; i++)
        oss << " " << t.get(static_cast<Resource>(i));
    oss << "\n";
    return oss.str();
}


World::World(const ServerConfig &config) : _map(config.width, config.height), _config(config)
{
    _initHandlers();
}

void World::_initHandlers()
{
    _handlers["Forward"]     = [this](Player &p, const std::string &)    { return _cmdForward(p); };
    _handlers["Right"]       = [this](Player &p, const std::string &)    { return _cmdRight(p); };
    _handlers["Left"]        = [this](Player &p, const std::string &)    { return _cmdLeft(p); };
    _handlers["Look"]        = [this](Player &p, const std::string &)    { return _cmdLook(p); };
    _handlers["Inventory"]   = [this](Player &p, const std::string &)    { return _cmdInventory(p); };
    _handlers["Take"]        = [this](Player &p, const std::string &arg) { return _cmdTake(p, arg); };
    _handlers["Set"]         = [this](Player &p, const std::string &arg) { return _cmdSet(p, arg); };
    _handlers["Broadcast"]   = [this](Player &p, const std::string &arg) { return _cmdBroadcast(p, arg); };
    _handlers["Fork"]        = [this](Player &p, const std::string &)    { return _cmdFork(p); };
    _handlers["Eject"]       = [this](Player &p, const std::string &)    { return _cmdEject(p); };
    _handlers["Incantation"] = [this](Player &p, const std::string &)    { return _cmdIncantation(p); };
    _handlers["Connect_nbr"] = [this](Player &p, const std::string &)    { return _cmdConnectNbr(p); };
}

void World::addPlayer(int fd, const std::string &team)
{
    static int nextId = 1;
    int x = std::rand() % _config.width;
    int y = std::rand() % _config.height;
    _players.emplace_back(nextId++, fd, x, y, team);

    Player &p = _players.back();
    if (onGuiEvent)
        onGuiEvent("pnw #" + std::to_string(p.id) + " " +
                   std::to_string(p.x) + " " +
                   std::to_string(p.y) + " " +
                   std::to_string(p.orientation) + " " +
                   std::to_string(p.level) + " " +
                   p.team + "\n");
}

bool World::hatchEgg(const std::string &team)
{
    auto it = std::find_if(_eggs.begin(), _eggs.end(),
        [&team](const Egg &e) { return e.team == team; });
    if (it == _eggs.end())
        return false;
    if (onGuiEvent)
        onGuiEvent("edi #" + std::to_string(it->id) + "\n");
    _eggs.erase(it);
    return true;
}

void World::removePlayer(int fd)
{
    Player *p = _getPlayer(fd);
    if (p && onGuiEvent && !p->isDead)
        onGuiEvent("pdi #" + std::to_string(p->id) + "\n");

    _players.erase(
        std::remove_if(_players.begin(), _players.end(),
            [fd](const Player &p) { return p.fd == fd; }),
        _players.end()
    );
}

Player *World::_getPlayer(int fd)
{
    for (auto &p : _players)
        if (p.fd == fd)
            return &p;
    return nullptr;
}

void World::update(int ticks)
{
    for (int t = 0; t < ticks; t++) {
        _spawnTimer++;
        if (_spawnTimer >= 20) {
            _map.spawnResources();
            _spawnTimer = 0;
        }

        for (auto it = _players.begin(); it != _players.end(); ) {
            Player &p = *it;

            if (p.isDead) {
                ++it;
                continue;
            }

            p.foodTimer--;
            if (p.foodTimer <= 0 && !p.isIncanting) {
                if (p.inventory[FOOD] > 0) {
                    p.inventory[FOOD]--;
                    p.foodTimer = 126;
                    if (onGuiEvent) onGuiEvent(guiPlayerInventory(p.id));
                } else {
                    p.isDead = true;
                    if (onPlayerEvent) onPlayerEvent(p.fd, "dead\n");
                    if (onGuiEvent) onGuiEvent("pdi #" + std::to_string(p.id) + "\n");
                    ++it;
                    continue;
                }
            }

            if (p.isDead) {
                ++it;
                continue;
            }

            if (p.isIncanting) {
                if (!p.actionQueue.empty() && p.actionQueue.front().name == "Incantation") {
                    // It's the initiator, proceed to decrement its timer
                } else {
                    ++it;
                    continue;
                }
            }

            if (!p.actionQueue.empty()) {
                Action &action = p.actionQueue.front();
                if (!action.isStarted) {
                    action.isStarted = true;
                    if (action.name == "Incantation") {
                        if (!_checkIncantationStart(p)) {
                            if (onPlayerEvent) onPlayerEvent(p.fd, "ko\n");
                            p.actionQueue.pop_front();
                            ++it;
                            continue;
                        }
                    }
                }

                if (action.remainingTicks > 0) {
                    action.remainingTicks--;
                }

                if (action.remainingTicks <= 0) {
                    std::string resp;
                    if (action.name == "Incantation") {
                        resp = _cmdIncantationEnd(p);
                    } else {
                        auto handler = _handlers.find(action.name);
                        if (handler != _handlers.end())
                            resp = handler->second(p, action.arg);
                    }
                    if (!resp.empty() && onPlayerEvent)
                        onPlayerEvent(p.fd, resp + "\n");
                    
                    p.actionQueue.pop_front();
                }
            }
            ++it;
        }
    }
}

static int getCommandCost(const std::string &name) {
    if (name == "Forward" || name == "Right" || name == "Left" ||
        name == "Look" || name == "Broadcast" || name == "Eject" ||
        name == "Take" || name == "Set") return 7;
    if (name == "Inventory") return 1;
    if (name == "Fork") return 42;
    if (name == "Incantation") return 300;
    return 0; // Connect_nbr
}

bool World::queueCommand(int playerFd, const std::string &cmd)
{
    Player *p = _getPlayer(playerFd);
    if (!p)
        return false;

    if (p->actionQueue.size() >= 10)
        return false;

    std::istringstream iss(cmd);
    std::string name;
    iss >> name;
    std::string arg;
    std::getline(iss >> std::ws, arg);

    auto it = _handlers.find(name);
    if (it == _handlers.end()) {
        if (onPlayerEvent)
            onPlayerEvent(playerFd, "suc\n");
        return true;
    }

    Action action;
    action.command = cmd;
    action.name = name;
    action.arg = arg;
    action.remainingTicks = getCommandCost(name);
    action.isStarted = false;

    p->actionQueue.push_back(action);
    return true;
}



std::vector<std::string> World::getGuiSnapshot() const
{
    std::vector<std::string> msgs;

    for (const auto &team : _config.teams)
        msgs.push_back("tna " + team + "\n");

    for (int y = 0; y < _map.getHeight(); y++)
        for (int x = 0; x < _map.getWidth(); x++)
            msgs.push_back(bctMsg(x, y, _map.getTile(x, y)));

    for (const auto &p : _players)
        msgs.push_back("pnw #" + std::to_string(p.id) + " " +
                        std::to_string(p.x) + " " + std::to_string(p.y) + " " +
                        std::to_string(p.orientation) + " " +
                        std::to_string(p.level) + " " + p.team + "\n");

    return msgs;
}

std::string World::guiMapSize() const
{
    return "msz " + std::to_string(_config.width) + " " + std::to_string(_config.height) + "\n";
}

std::string World::guiTileContent(int x, int y) const
{
    if (x < 0 || x >= _map.getWidth() || y < 0 || y >= _map.getHeight()) return "";
    return bctMsg(x, y, _map.getTile(x, y));
}

std::string World::guiMapContent() const
{
    std::ostringstream oss;
    for (int y = 0; y < _map.getHeight(); y++)
        for (int x = 0; x < _map.getWidth(); x++)
            oss << bctMsg(x, y, _map.getTile(x, y));
    return oss.str();
}

std::string World::guiTeamNames() const
{
    std::ostringstream oss;
    for (const auto &team : _config.teams)
        oss << "tna " << team << "\n";
    return oss.str();
}

std::string World::guiPlayerPosition(int id) const
{
    for (const auto &p : _players) {
        if (p.id == id)
            return "ppo #" + std::to_string(p.id) + " " + std::to_string(p.x) + " " + std::to_string(p.y) + " " + std::to_string(p.orientation) + "\n";
    }
    return "";
}

std::string World::guiPlayerLevel(int id) const
{
    for (const auto &p : _players) {
        if (p.id == id)
            return "plv #" + std::to_string(p.id) + " " + std::to_string(p.level) + "\n";
    }
    return "";
}



std::string World::guiPlayerInventory(int id) const
{
    for (const auto &p : _players) {
        if (p.id != id)
            continue;
        std::ostringstream oss;
        oss << "pin #" << p.id << " " << p.x << " " << p.y;
        for (int i = 0; i < RESOURCE_COUNT; i++)
            oss << " " << p.inventory[i];
        oss << "\n";
        return oss.str();
    }
    return "";
}

std::string World::_cmdForward(Player &p)
{
    switch (p.orientation) {
        case NORTH: p.y = _map.wrapY(p.y - 1); break;
        case SOUTH: p.y = _map.wrapY(p.y + 1); break;
        case EAST:  p.x = _map.wrapX(p.x + 1); break;
        case WEST:  p.x = _map.wrapX(p.x - 1); break;
    }
    if (onGuiEvent)
        onGuiEvent("ppo #" + std::to_string(p.id) + " " +
                   std::to_string(p.x) + " " +
                   std::to_string(p.y) + " " +
                   std::to_string(p.orientation) + "\n");
    return "ok";
}

std::string World::_cmdRight(Player &p)
{
    p.turnRight();
    if (onGuiEvent)
        onGuiEvent("ppo #" + std::to_string(p.id) + " " +
                   std::to_string(p.x) + " " +
                   std::to_string(p.y) + " " +
                   std::to_string(p.orientation) + "\n");
    return "ok";
}

std::string World::_cmdLeft(Player &p)
{
    p.turnLeft();
    if (onGuiEvent)
        onGuiEvent("ppo #" + std::to_string(p.id) + " " +
                   std::to_string(p.x) + " " +
                   std::to_string(p.y) + " " +
                   std::to_string(p.orientation) + "\n");
    return "ok";
}

std::string World::_cmdLook(Player &p)
{
    int fx = 0;
    int fy = 0;
    int rx = 0;
    int ry = 0;
    switch (p.orientation) {
        case NORTH: fy = -1; rx =  1; break;
        case SOUTH: fy =  1; rx = -1; break;
        case EAST:  fx =  1; ry =  1; break;
        case WEST:  fx = -1; ry = -1; break;
    }

    std::ostringstream oss;
    oss << "[";
    bool firstTile = true;

    for (int dist = 0; dist <= p.level; dist++) {
        int cx = p.x + fx * dist;
        int cy = p.y + fy * dist;

        for (int side = -dist; side <= dist; side++) {
            if (!firstTile)
                oss << ",";
            firstTile = false;

            int tx = _map.wrapX(cx + rx * side);
            int ty = _map.wrapY(cy + ry * side);
            const Tile &tile = _map.getTile(tx, ty);
            bool firstItem = true;

            for (const auto &other : _players) {
                if (other.x == tx && other.y == ty) {
                    if (!firstItem)
                        oss << " ";
                    oss << "player";
                    firstItem = false;
                }
            }

            for (int r = 0; r < RESOURCE_COUNT; r++) {
                int qty = tile.get(static_cast<Resource>(r));
                for (int q = 0; q < qty; q++) {
                    if (!firstItem)
                        oss << " ";
                    oss << RESOURCE_NAMES[r];
                    firstItem = false;
                }
            }
        }
    }
    oss << "]";
    return oss.str();
}

std::string World::_cmdInventory(Player &p)
{
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < RESOURCE_COUNT; i++) {
        if (i > 0)
            oss << ", ";
        oss << RESOURCE_NAMES[i] << " " << p.inventory[i];
    }
    oss << "]";
    return oss.str();
}

std::string World::_cmdTake(Player &p, const std::string &res)
{
    int r = resourceFromName(res);
    if (r < 0)
        return "ko";

    Tile &tile = _map.getTile(p.x, p.y);
    if (!tile.remove(static_cast<Resource>(r)))
        return "ko";

    p.inventory[r]++;

    if (onGuiEvent) {
        onGuiEvent("pgt #" + std::to_string(p.id) + " " + std::to_string(r) + "\n");
        onGuiEvent(bctMsg(p.x, p.y, tile));
        onGuiEvent(guiPlayerInventory(p.id));
    }
    return "ok";
}

std::string World::_cmdSet(Player &p, const std::string &res)
{
    int r = resourceFromName(res);
    if (r < 0)
        return "ko";

    if (p.inventory[r] <= 0)
        return "ko";

    p.inventory[r]--;
    Tile &tile = _map.getTile(p.x, p.y);
    tile.add(static_cast<Resource>(r));

    if (onGuiEvent) {
        onGuiEvent("pdr #" + std::to_string(p.id) + " " + std::to_string(r) + "\n");
        onGuiEvent(bctMsg(p.x, p.y, tile));
        onGuiEvent(guiPlayerInventory(p.id));
    }
    return "ok";
}

static int broadcastDirection(int sx, int sy, int rx, int ry, int width, int height, Orientation receiverOrientation)
{
    int dx = sx - rx;
    int dy = sy - ry;
    if (dx >  width / 2)
        dx -= width;
    if (dx < -width / 2)
        dx += width;
    if (dy >  height / 2)
        dy -= height;
    if (dy < -height / 2)
        dy += height;
    if (dx == 0 && dy == 0)
        return 0;

    double angle = std::atan2(dy, dx);
    double orientAngle = 0.0;
    switch (receiverOrientation) {
        case NORTH: orientAngle = -M_PI / 2; break;
        case EAST:  orientAngle =  0;         break;
        case SOUTH: orientAngle =  M_PI / 2; break;
        case WEST:  orientAngle =  M_PI;      break;
    }

    double rel = angle - orientAngle;
    while (rel >  M_PI)
        rel -= 2 * M_PI;
    while (rel < -M_PI)
        rel += 2 * M_PI;

    int sector = static_cast<int>(std::round(rel / (M_PI / 4)));
    if (sector < 0) sector += 8;
    sector = sector % 8;

    static const int SECTOR_TO_K[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    return SECTOR_TO_K[sector];
}

std::string World::_cmdBroadcast(Player &p, const std::string &msg)
{
    for (auto &other : _players) {
        if (other.fd == p.fd)
            continue;
        int k = broadcastDirection(p.x, p.y, other.x, other.y, _config.width, _config.height, other.orientation);
        std::string event = "message " + std::to_string(k) + ", " + msg + "\n";
        if (onPlayerEvent)
            onPlayerEvent(other.fd, event);
    }
    if (onGuiEvent)
        onGuiEvent("pbc #" + std::to_string(p.id) + " " + msg + "\n");
    return "ok";
}

std::string World::_cmdConnectNbr(Player &p)
{
    int slots = 0;
    for (const auto &egg : _eggs)
        if (egg.team == p.team)
            slots++;
    return std::to_string(slots);
}

std::string World::_cmdFork(Player &p)
{
    Egg egg;
    egg.id   = _nextEggId++;
    egg.x    = p.x;
    egg.y    = p.y;
    egg.team = p.team;
    _eggs.push_back(egg);

    if (onGuiEvent) {
        onGuiEvent("pfk #" + std::to_string(p.id) + "\n");
        onGuiEvent("enw #" + std::to_string(egg.id) + " #" +
                   std::to_string(p.id) + " " +
                   std::to_string(egg.x) + " " +
                   std::to_string(egg.y) + "\n");
    }
    return "ok";
}

std::string World::_cmdEject(Player &p)
{
    bool ejected = false;

    for (auto &other : _players) {
        if (other.fd == p.fd)
            continue;
        if (other.x != p.x || other.y != p.y)
            continue;

        switch (p.orientation) {
            case NORTH: other.y = _map.wrapY(other.y - 1); break;
            case SOUTH: other.y = _map.wrapY(other.y + 1); break;
            case EAST:  other.x = _map.wrapX(other.x + 1); break;
            case WEST:  other.x = _map.wrapX(other.x - 1); break;
        }

        if (other.isIncanting)
            other.isIncanting = false;

        int k = broadcastDirection(p.x, p.y, other.x, other.y,
                                   _config.width, _config.height, other.orientation);
        if (onPlayerEvent)
            onPlayerEvent(other.fd, "eject: " + std::to_string(k) + "\n");

        if (onGuiEvent)
            onGuiEvent("pex #" + std::to_string(other.id) + "\n");

        ejected = true;
    }
    _eggs.erase(
        std::remove_if(_eggs.begin(), _eggs.end(),
            [&](const Egg &e) {
                if (e.x == p.x && e.y == p.y) {
                    if (onGuiEvent)
                        onGuiEvent("edi #" + std::to_string(e.id) + "\n");
                    return true;
                }
                return false;
            }),
        _eggs.end()
    );

    return ejected ? "ok" : "ko";
}

std::string World::_cmdIncantation(Player &/*p*/)
{
    return "";
}

bool World::_checkIncantationStart(Player &p)
{
    if (p.level < 1 || p.level > 7) return false;
    int lvlIdx = p.level - 1;
    const int *req = ELEVATION[lvlIdx];

    int playersOnTile = 0;
    for (const auto &other : _players)
        if (other.x == p.x && other.y == p.y && other.level == p.level)
            playersOnTile++;

    if (playersOnTile < req[0]) return false;

    const Tile &tile = _map.getTile(p.x, p.y);
    for (int r = 0; r < RESOURCE_COUNT - 1; r++)
        if (tile.get(static_cast<Resource>(r + 1)) < req[r + 1]) return false;

    std::ostringstream picMsg;
    picMsg << "pic " << p.x << " " << p.y << " " << p.level;
    for (auto &other : _players) {
        if (other.x == p.x && other.y == p.y && other.level == p.level) {
            other.isIncanting = true;
            picMsg << " #" << other.id;
            if (onPlayerEvent) onPlayerEvent(other.fd, "Elevation underway\n");
        }
    }
    if (onGuiEvent) onGuiEvent(picMsg.str() + "\n");
    return true;
}

std::string World::_cmdIncantationEnd(Player &p)
{
    auto unfreeze = [this, &p]() {
        for (auto &other : _players)
            if (other.x == p.x && other.y == p.y && other.level == p.level)
                other.isIncanting = false;
        if (onGuiEvent) onGuiEvent("pie " + std::to_string(p.x) + " " + std::to_string(p.y) + " 0\n");
    };

    if (p.level < 1 || p.level > 7) {
        unfreeze(); return "ko";
    }

    int lvlIdx = p.level - 1;
    const int *req = ELEVATION[lvlIdx];

    int playersOnTile = 0;
    for (const auto &other : _players)
        if (other.x == p.x && other.y == p.y && other.level == p.level)
            playersOnTile++;

    if (playersOnTile < req[0]) {
        unfreeze(); return "ko";
    }

    const Tile &tile = _map.getTile(p.x, p.y);
    for (int r = 0; r < RESOURCE_COUNT - 1; r++)
        if (tile.get(static_cast<Resource>(r + 1)) < req[r + 1]) {
            unfreeze(); return "ko";
        }

    Tile &t = _map.getTile(p.x, p.y);
    for (int r = 0; r < RESOURCE_COUNT - 1; r++)
        for (int q = 0; q < req[r + 1]; q++)
            t.remove(static_cast<Resource>(r + 1));

    int startLevel = p.level;
    for (auto &other : _players) {
        if (other.x == p.x && other.y == p.y && other.level == startLevel) {
            other.level++;
            other.isIncanting = false;
            if (onGuiEvent)
                onGuiEvent("plv #" + std::to_string(other.id) + " " + std::to_string(other.level) + "\n");
            if (other.fd != p.fd && onPlayerEvent)
                onPlayerEvent(other.fd, "Current level: " + std::to_string(other.level) + "\n");
        }
    }

    if (onGuiEvent) {
        onGuiEvent("pie " + std::to_string(p.x) + " " + std::to_string(p.y) + " 1\n");
        onGuiEvent(bctMsg(p.x, p.y, _map.getTile(p.x, p.y)));
    }

    for (const auto &team : _config.teams) {
        int count = 0;
        for (const auto &pl : _players)
            if (pl.team == team && pl.level == 8)
                count++;
        if (count >= 6) {
            if (onGuiEvent) onGuiEvent("seg " + team + "\n");
        }
    }
    return "Current level: " + std::to_string(p.level);
}