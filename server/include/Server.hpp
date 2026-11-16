#pragma once

#include "Args.hpp"
#include "Client.hpp"
#include "World.hpp"
#include <vector>
#include <unordered_map>
#include <functional>
#include <poll.h>
#include <chrono>

class Server {
public:
    Server(const ServerConfig &config);
    ~Server();

    void run();

private:
    int _fd;
    ServerConfig _config;
    std::vector<struct pollfd> _fds;
    std::vector<Client> _clients;
    std::unordered_map<std::string, int> _slots;
    World _world;
    std::chrono::steady_clock::time_point _lastTick;
    std::unordered_map<std::string, std::function<void(Client &, const std::string &)>> _guiRequests;

    void _createSocket();
    void _bindAndListen();
    void _acceptClient();
    bool _handleClient(std::size_t index);
    void _processHandshake(std::size_t index, const std::string &line);
    void _processGuiRequest(Client &client, const std::string &line);
    void _initGuiRequests();
    bool _flushClient(std::size_t index);
    void _sendGuiBootstrap(Client &client);
    void _broadcastToGui(const std::string &msg);
    void _sendToPlayer(int fd, const std::string &msg);
};
