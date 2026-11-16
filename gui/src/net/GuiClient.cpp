#include "net/GuiClient.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sstream>

static int parseId(const std::string &tok)
{
    if (!tok.empty() && tok[0] == '#')
        return std::atoi(tok.c_str() + 1);
    return std::atoi(tok.c_str());
}

static const char *resName(int i)
{
    static const char *N[] = {"food", "linemate", "deraumere", "sibur",
                              "mendiane", "phiras", "thystame"};
    return (i >= 0 && i < 7) ? N[i] : "?";
}

void GuiClient::_logEvent(int id, const std::string &action, int kind)
{
    std::string team;
    auto it = _state.players.find(id);
    if (it != _state.players.end())
        team = it->second.team;
    _state.log.push_back({id, team, action, kind});
    _state.logTotal++;
    if (_state.log.size() > 100)
        _state.log.pop_front();
}

GuiClient::~GuiClient()
{
    disconnect();
}

void GuiClient::disconnect()
{
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
}

void GuiClient::send(const std::string &line)
{
    if (_fd < 0)
        return;
    std::string out = line;
    out.push_back('\n');
    ssize_t n = ::write(_fd, out.data(), out.size());
    (void)n;

    _state.rawFeed.push_back(">> " + line);
    if (_state.rawFeed.size() > 200)
        _state.rawFeed.pop_front();
}

bool GuiClient::connect(const std::string &host, int port, std::string &err)
{
    struct addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        err = "cannot resolve host '" + host + "'";
        return false;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        err = std::string("socket() failed: ") + std::strerror(errno);
        freeaddrinfo(res);
        return false;
    }

    struct timeval tv{};
    tv.tv_sec = 5;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        err = std::string("connect() failed: ") + std::strerror(errno);
        close(fd);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    std::string welcome;
    char ch;
    while (welcome.size() < 64) {
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) {
            err = "no WELCOME from server (timeout or closed)";
            close(fd);
            return false;
        }
        if (ch == '\n')
            break;
        welcome += ch;
    }
    if (welcome.rfind("WELCOME", 0) != 0) {
        err = "unexpected greeting: '" + welcome + "'";
        close(fd);
        return false;
    }

    const char *graphic = "GRAPHIC\n";
    if (write(fd, graphic, std::strlen(graphic)) < 0) {
        err = std::string("write() failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    _fd = fd;
    _readBuf.clear();
    _state = GuiState{};
    _state.rawFeed.push_back("<< " + welcome);
    _state.rawFeed.push_back(">> GRAPHIC");
    return true;
}

void GuiClient::poll()
{
    if (_fd < 0)
        return;

    char buf[8192];
    while (true) {
        ssize_t n = read(_fd, buf, sizeof(buf));
        if (n > 0) {
            _readBuf.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            disconnect();
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        if (errno == EINTR)
            continue;
        disconnect();
        return;
    }

    static const int MAX_LINES = 3000;
    int processed = 0;
    std::size_t nl;
    while (processed < MAX_LINES &&
           (nl = _readBuf.find('\n')) != std::string::npos) {
        std::string line = _readBuf.substr(0, nl);
        _readBuf.erase(0, nl + 1);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty()) {

            bool isHF = (line.size() >= 3 &&
                        ((line[0]=='b' && line[1]=='c' && line[2]=='t') ||
                         (line[0]=='p' && line[1]=='p' && line[2]=='o')));
            if (!isHF) {
                _state.rawFeed.push_back("<< " + line);
                if (_state.rawFeed.size() > 200)
                    _state.rawFeed.pop_front();
            }
            _processLine(line);
            ++processed;
        }
    }
}

void GuiClient::_processLine(const std::string &line)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "msz") {
        int w, h;
        if (iss >> w >> h)
            _state.resizeGrid(w, h);

    } else if (cmd == "sgt") {
        int t;
        if (iss >> t)
            _state.timeUnit = t;

    } else if (cmd == "sst") {
        int ticks;
        if (iss >> ticks && _state.timeUnit > 0)
            _state.secondsToReset = (float)ticks / (float)_state.timeUnit;

    } else if (cmd == "tna") {
        std::string team;
        if (iss >> team)
            _state.teams.push_back(team);

    } else if (cmd == "bct") {
        int x, y;
        if (iss >> x >> y && _state.inBounds(x, y)) {
            GuiTile &t = _state.tileAt(x, y);
            for (int i = 0; i < GUI_RES_COUNT; ++i)
                iss >> t.res[i];
            _state.resDirty = true;
        }

    } else if (cmd == "pnw") {
        GuiPlayer p;
        std::string idTok;
        iss >> idTok >> p.x >> p.y >> p.orientation >> p.level >> p.team;
        p.id = parseId(idTok);
        _state.players[p.id] = p;

    } else if (cmd == "ppo") {
        std::string idTok;
        int x, y, o;
        if (iss >> idTok >> x >> y >> o) {
            auto it = _state.players.find(parseId(idTok));
            if (it != _state.players.end()) {
                it->second.x = x;
                it->second.y = y;
                it->second.orientation = o;
            }
        }

    } else if (cmd == "plv") {
        std::string idTok;
        int lvl;
        if (iss >> idTok >> lvl) {
            int id = parseId(idTok);
            auto it = _state.players.find(id);
            if (it != _state.players.end())
                it->second.level = lvl;
            _logEvent(id, "reached level " + std::to_string(lvl), 1);
        }

    } else if (cmd == "pgt") {
        std::string idTok;
        int r;
        if (iss >> idTok >> r)
            _logEvent(parseId(idTok), std::string("grabbed ") + resName(r), 0);

    } else if (cmd == "pin") {
        std::string idTok;
        int x, y;
        if (iss >> idTok >> x >> y) {
            auto it = _state.players.find(parseId(idTok));
            if (it != _state.players.end()) {
                it->second.x = x;
                it->second.y = y;
                for (int i = 0; i < GUI_RES_COUNT; ++i)
                    iss >> it->second.inv[i];
                it->second.invKnown = true;
            }
        }

    } else if (cmd == "pdi") {
        std::string idTok;
        if (iss >> idTok) {
            int id = parseId(idTok);
            _logEvent(id, "died", 2);
            _state.players.erase(id);
        }

    } else if (cmd == "enw") {
        GuiEgg e;
        std::string eggTok, playerTok;
        if (iss >> eggTok >> playerTok >> e.x >> e.y) {
            e.id = parseId(eggTok);
            auto pit = _state.players.find(parseId(playerTok));
            if (pit != _state.players.end())
                e.team = pit->second.team;
            _state.eggs[e.id] = e;
        }

    } else if (cmd == "edi") {
        std::string eggTok;
        if (iss >> eggTok)
            _state.eggs.erase(parseId(eggTok));

    } else if (cmd == "pic") {
        GuiIncantation fx;
        int lvl;
        if (iss >> fx.x >> fx.y >> lvl) {
            fx.resolved = false;
            _incantations.push_back(fx);
            std::string idTok;
            int firstId = -1;
            while (iss >> idTok) {
                int id = parseId(idTok);
                if (firstId < 0) firstId = id;
                auto it = _state.players.find(id);
                if (it != _state.players.end())
                    it->second.incanting = true;
            }
            if (firstId >= 0)
                _logEvent(firstId, "begins an incantation", 3);
        }

    } else if (cmd == "pie") {
        GuiIncantation fx;
        int result = 0;
        if (iss >> fx.x >> fx.y >> result) {
            fx.resolved = true;
            fx.success  = (result != 0);
            _incantations.push_back(fx);
        }
        for (auto &kv : _state.players)
            if (kv.second.x == fx.x && kv.second.y == fx.y)
                kv.second.incanting = false;

    } else if (cmd == "pex") {
        std::string idTok;
        if (iss >> idTok) {
            int id = parseId(idTok);
            auto it = _state.players.find(id);
            if (it != _state.players.end())
                it->second.ejected = true;
            _logEvent(id, "was ejected", 4);
        }

    } else if (cmd == "seg") {
        std::string team;
        if (iss >> team)
            _state.winner = team;
    }
}
