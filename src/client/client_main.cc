#include "../../include/client.h"

int main() {
    Client client("127.0.0.1", 8080);
    if (!client.Start()) {
        return 1;
    }

    client.Run();
    return 0;
}