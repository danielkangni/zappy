#include "../include/Args.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>

void usage(const char *bin)
{
    std::cerr << "USAGE: " << bin
              << " -p port -x width -y height -n name1 name2 ... -c clientsNb -f freq\n";
    exit(84);
}

ServerConfig parseArgs(int argc, char **argv)
{
    ServerConfig config;

    if (argc < 2)
        usage(argv[0]);

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-p" && i + 1 < argc) {
            config.port = std::atoi(argv[++i]);
        } else if (arg == "-x" && i + 1 < argc) {
            config.width = std::atoi(argv[++i]);
        } else if (arg == "-y" && i + 1 < argc) {
            config.height = std::atoi(argv[++i]);
        } else if (arg == "-c" && i + 1 < argc) {
            config.clients = std::atoi(argv[++i]);
        } else if (arg == "-f" && i + 1 < argc) {
            config.freq = std::atoi(argv[++i]);
        } else if (arg == "-n") {
            while (i + 1 < argc && argv[i + 1][0] != '-')
                config.teams.push_back(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
        }
    }
    if (config.port == -1 || config.width == -1 || config.height == -1 ||
        config.clients == -1 || config.teams.empty()) {
        std::cerr << "Error: missing required argument\n";
        usage(argv[0]);
    }
    if (config.port <= 0 || config.port > 65535) {
        std::cerr << "Error: port must be between 1 and 65535\n";
        usage(argv[0]);
    }
    if (config.width < 1) {
        std::cerr << "Error: width must be > 0\n";
        usage(argv[0]);
    }
    if (config.width > 42) {
        std::cerr << "Warning: width clamped to 42\n";
        config.width = 42;
    }
    if (config.height < 1) {
        std::cerr << "Error: height must be > 0\n";
        usage(argv[0]);
    }
    if (config.height > 42) {
        std::cerr << "Warning: height clamped to 42\n";
        config.height = 42;
    }
    if (config.clients < 1) {
        std::cerr << "Error: clientsNb must be > 0\n";
        usage(argv[0]);
    }
    if (config.freq <= 0) {
        std::cerr << "Error: freq must be > 0\n";
        usage(argv[0]);
    }
    if (config.freq > 500) {
        std::cerr << "Warning: freq clamped to 500\n";
        config.freq = 500;
    }
    return config;
}
