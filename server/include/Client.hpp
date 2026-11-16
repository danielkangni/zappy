#pragma once

#include <string>

enum class ClientState {
    WAITING_TEAM_NAME,
    AI,
    GUI,
    CLOSING
};

class Client {
public:
    Client(int fd);

    int getFd() const;

    ClientState getState() const;
    void setState(ClientState state);

    const std::string &getTeam() const;
    void setTeam(const std::string &team);

    const std::string &getReadBuffer() const;
    void appendToReadBuffer(const char *data, std::size_t len);
    void clearReadBuffer();
    bool extractLine(std::string &line);

    const std::string &getWriteBuffer() const;
    void queueWrite(const std::string &data);
    void consumeWriteBuffer(std::size_t len);
    bool hasPendingWrite() const;

private:
    int _fd;
    std::string _readBuffer;
    std::string _writeBuffer;
    ClientState _state = ClientState::WAITING_TEAM_NAME;
    std::string _team;
};
