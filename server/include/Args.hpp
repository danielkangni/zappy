#pragma once

#include <string>
#include <vector>

struct ServerConfig {
    int port = -1;
    int width = -1;
    int height = -1;
    int clients = -1;
    int freq = 100;
    std::vector<std::string> teams;
};

ServerConfig parseArgs(int argc, char **argv);
void usage(const char *bin);
