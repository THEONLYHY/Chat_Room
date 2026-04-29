#include "server.h"
#include "logger.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    // ./server 8888
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "invalid port\n";
        return 1;
    }

    if (!Logger::Init()) {
        return false;
    }

    Server server(port);
    if (!server.Start()) {
        std::cerr << "server start failed\n";
        return 1;
    }

    server.Run();
    return 0;
}
