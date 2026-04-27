#include "../../include/socket.h"

#include <cstring>
#include <iostream>

int main() {
    Socket socket;
    if (!socket.Create()) {
        return 1;
    }
    if (!socket.Connect("127.0.0.1", 8080)) {
        return 1;
    }

    const char* msg = "hello socket";
    socket.Send(msg, strlen(msg));

    char buf[1024] = {0};
    ssize_t n = socket.Recv(buf, sizeof(buf) - 1);

    if (n > 0) {
        std::cout << "server echo : " << buf << '\n';
    }

    return 0;
}