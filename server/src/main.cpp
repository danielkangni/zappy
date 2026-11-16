#include "../include/Args.hpp"
#include "../include/Server.hpp"
#include <iostream>

int main(int argc, char **argv)
{
    ServerConfig config = parseArgs(argc, argv);

    try {
        Server server(config);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 84;
    }
    return 0;
}
