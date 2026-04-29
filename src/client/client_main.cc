#include "../../include/client.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <server_ip> <port>\n";
    return 1;
  }

  std::string server_ip = argv[1];
  int port = std::atoi(argv[2]);
  if (port <= 0 || port > 65535) {
    std::cerr << "invalid port\n";
    return 1;
  }

  Client client(server_ip, port);
  if (!client.Start()) {
    std::cerr << "client start failed\n";
    return 1;
  }

  client.Run();
  return 0;
}
