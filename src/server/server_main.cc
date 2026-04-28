#include "../../include/server.h"

int main() {
    Server server(8080);
    if (!server.Start()) {
        return 1;
    }
    server.Run();
    return 0;
}