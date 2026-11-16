#pragma once

#include "net/GuiState.hpp"
#include <string>
#include <deque>

class GuiClient {
public:
    GuiClient() = default;
    ~GuiClient();

    GuiClient(const GuiClient &) = delete;
    GuiClient &operator=(const GuiClient &) = delete;

    bool connect(const std::string &host, int port, std::string &err);

    void poll();

    void disconnect();

    void send(const std::string &line);

    bool connected() const { return _fd >= 0; }
    const GuiState &state() const { return _state; }
    GuiState &state() { return _state; }

    std::deque<GuiIncantation> &incantationEvents() { return _incantations; }

private:
    int       _fd = -1;
    GuiState  _state;
    std::string _readBuf;
    std::deque<GuiIncantation> _incantations;

    void _processLine(const std::string &line);

    void _logEvent(int id, const std::string &action, int kind);
};
