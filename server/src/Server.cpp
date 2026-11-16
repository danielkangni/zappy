#include "../include/Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <sstream>

Server::Server(const ServerConfig &config) : _fd(-1), _config(config), _world(_config)
{
    _createSocket();
    _bindAndListen();

    for (const auto &team : _config.teams)
        _slots[team] = _config.clients;

    _world.onGuiEvent = [this](const std::string &msg) {
        _broadcastToGui(msg);
    };
    _world.onPlayerEvent = [this](int fd, const std::string &msg) {
        if (msg == "dead\n") {
            for (auto &c : _clients) {
                if (c.getFd() == fd) {
                    c.queueWrite(msg);
                    c.setState(ClientState::CLOSING);
                    break;
                }
            }
        } else {
            _sendToPlayer(fd, msg);
        }
    };

    _lastTick = std::chrono::steady_clock::now();
    _initGuiRequests();
}

void Server::_initGuiRequests()
{
    _guiRequests["msz"] = [this](Client &c, const std::string &) {
        c.queueWrite(_world.guiMapSize());
    };
    _guiRequests["tna"] = [this](Client &c, const std::string &) {
        c.queueWrite(_world.guiTeamNames());
    };
    _guiRequests["mct"] = [this](Client &c, const std::string &) {
        c.queueWrite(_world.guiMapContent());
    };
    _guiRequests["bct"] = [this](Client &c, const std::string &arg) {
        std::istringstream iss(arg);
        int x, y;
        if (iss >> x >> y)
            c.queueWrite(_world.guiTileContent(x, y));
    };
    _guiRequests["ppo"] = [this](Client &c, const std::string &arg) {
        if (!arg.empty() && arg[0] == '#') {
            std::string resp = _world.guiPlayerPosition(std::stoi(arg.substr(1)));
            if (!resp.empty())
                c.queueWrite(resp);
        }
    };
    _guiRequests["plv"] = [this](Client &c, const std::string &arg) {
        if (!arg.empty() && arg[0] == '#') {
            std::string resp = _world.guiPlayerLevel(std::stoi(arg.substr(1)));
            if (!resp.empty())
                c.queueWrite(resp);
        }
    };
    _guiRequests["pin"] = [this](Client &c, const std::string &arg) {
        if (!arg.empty() && arg[0] == '#') {
            std::string resp = _world.guiPlayerInventory(std::stoi(arg.substr(1)));
            if (!resp.empty())
                c.queueWrite(resp);
        }
    };
    _guiRequests["sgt"] = [this](Client &c, const std::string &) {
        c.queueWrite("sgt " + std::to_string(_config.freq) + "\n");
    };
    _guiRequests["sst"] = [this](Client &c, const std::string &arg) {
        int freq = std::stoi(arg);
        if (freq > 0)
            _config.freq = freq;
        c.queueWrite("sst " + std::to_string(_config.freq) + "\n");
    };
}

void Server::_processGuiRequest(Client &client, const std::string &line)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    std::string arg;
    std::getline(iss >> std::ws, arg);

    auto it = _guiRequests.find(cmd);
    if (it != _guiRequests.end()) {
        try {
            it->second(client, arg);
        } catch (const std::exception &) {
        }
    }
}

void Server::_broadcastToGui(const std::string &msg)
{
    for (auto &client : _clients)
        if (client.getState() == ClientState::GUI)
            client.queueWrite(msg);
}

void Server::_sendToPlayer(int fd, const std::string &msg)
{
    for (auto &client : _clients)
        if (client.getFd() == fd) {
            client.queueWrite(msg);
            return;
        }
}

void Server::_sendGuiBootstrap(Client &client)
{
    client.queueWrite("msz " + std::to_string(_config.width) + " " +
                       std::to_string(_config.height) + "\n");
    for (const auto &msg : _world.getGuiSnapshot())
        client.queueWrite(msg);
    client.queueWrite("sgt " + std::to_string(_config.freq) + "\n");
}

void Server::_bindAndListen()
{
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_config.port);

    if (bind(_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        throw std::runtime_error("bind() failed");
    if (listen(_fd, SOMAXCONN) == -1)
        throw std::runtime_error("listen() failed");

    std::cout << "Server listening on port " << _config.port << std::endl;
}

void Server::_createSocket()
{
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (fcntl(_fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl() failed");
}

void Server::_acceptClient()
{
    int clientFd = accept(_fd, nullptr, nullptr);
    if (clientFd == -1)
        return;

    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        close(clientFd);
        return;
    }
    _fds.push_back({clientFd, POLLIN, 0});
    _clients.push_back(Client(clientFd));
    _clients.back().queueWrite("WELCOME\n");
    std::cout << "Client connected: fd=" << clientFd << std::endl;
}

void Server::_processHandshake(std::size_t index, const std::string &line)
{
    Client &client = _clients[index - 1];

    if (line == "GRAPHIC") {
        client.setState(ClientState::GUI);
        _sendGuiBootstrap(client);
        return;
    }

    auto it = _slots.find(line);
    if (it != _slots.end() && it->second > 0) {
        it->second--;
        client.setState(ClientState::AI);
        client.setTeam(line);
        client.queueWrite(std::to_string(it->second) + "\n");
        client.queueWrite(std::to_string(_config.width) + " " +
                           std::to_string(_config.height) + "\n");
        _world.addPlayer(client.getFd(), line);
        return;
    }

    // No initial slot left for this team: try hatching one of its Fork eggs
    // instead of rejecting the connection outright.
    if (it != _slots.end() && _world.hatchEgg(line)) {
        client.setState(ClientState::AI);
        client.setTeam(line);
        client.queueWrite("0\n");
        client.queueWrite(std::to_string(_config.width) + " " +
                           std::to_string(_config.height) + "\n");
        _world.addPlayer(client.getFd(), line);
        return;
    }

    client.queueWrite("ko\n");
    client.setState(ClientState::CLOSING);
}

bool Server::_handleClient(std::size_t index)
{
    char buf[4096];
    int n = read(_fds[index].fd, buf, sizeof(buf));
    std::size_t clientIndex = index - 1;

    if (n <= 0) {
        std::cout << "Client disconnected: fd=" << _fds[index].fd << std::endl;
        if (_clients[clientIndex].getState() == ClientState::AI)
            _world.removePlayer(_fds[index].fd);
        close(_fds[index].fd);
        _fds.erase(_fds.begin() + index);
        _clients.erase(_clients.begin() + clientIndex);
        return true;
    }
    Client &client = _clients[clientIndex];
    client.appendToReadBuffer(buf, n);

    std::string line;
    while (client.extractLine(line)) {
        if (client.getState() == ClientState::WAITING_TEAM_NAME)
            _processHandshake(index, line);
        else if (client.getState() == ClientState::AI)
            _world.queueCommand(client.getFd(), line);
        else if (client.getState() == ClientState::GUI)
            _processGuiRequest(client, line);
    }

    return false;
}

bool Server::_flushClient(std::size_t index)
{
    std::size_t clientIndex = index - 1;
    Client &client = _clients[clientIndex];
    const std::string &buffer = client.getWriteBuffer();

    int n = write(_fds[index].fd, buffer.data(), buffer.size());
    if (n > 0)
        client.consumeWriteBuffer(n);

    if (client.getState() == ClientState::CLOSING && !client.hasPendingWrite()) {
        close(_fds[index].fd);
        _fds.erase(_fds.begin() + index);
        _clients.erase(_clients.begin() + clientIndex);
        return true;
    }
    return false;
}

void Server::run()
{
    _fds.push_back({_fd, POLLIN, 0});

    while (true) {
        for (std::size_t i = 1; i < _fds.size(); i++) {
            _fds[i].events = POLLIN;
            if (_clients[i - 1].hasPendingWrite())
                _fds[i].events |= POLLOUT;
        }

        double tickDuration = 1.0 / _config.freq;
        double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - _lastTick).count();
        int ticks = static_cast<int>(elapsed / tickDuration);
        if (ticks > 0) {
            _world.update(ticks);
            _lastTick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(ticks * tickDuration));
        }

        double remaining = tickDuration - std::chrono::duration<double>(
            std::chrono::steady_clock::now() - _lastTick).count();
        int timeoutMs = static_cast<int>(remaining * 1000.0);
        if (timeoutMs < 1)
            timeoutMs = 1;

        if (poll(_fds.data(), _fds.size(), timeoutMs) == -1)
            throw std::runtime_error("poll() failed");

        if (_fds[0].revents & POLLIN)
            _acceptClient();

        for (std::size_t i = 1; i < _fds.size(); i++) {
            bool disconnected = false;

            if (_fds[i].revents & POLLIN)
                disconnected = _handleClient(i);
            if (!disconnected && (_fds[i].revents & POLLOUT))
                disconnected = _flushClient(i);
            if (disconnected)
                i--;
        }
    }
}

Server::~Server()
{
    for (auto &pfd : _fds)
        close(pfd.fd);
    if (_fd != -1)
        close(_fd);
}
