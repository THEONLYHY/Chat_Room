#include "../../include/server.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "invalid port\n";
        return 1;
    }

    Server server(port);
    if (!server.Start()) {
        std::cerr << "server start failed\n";
        return 1;
    }

    server.Run();
    return 0;
}
