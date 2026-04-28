// #include "../../include/epoll.h"
// #include "../../include/socket.h"
// #include "../../include/tcp_connection.h"

// #include <cerrno>       // errno
// #include <csignal>      // signal, SIGPIPE, SIG_IGN
// #include <cstring>      // strerror
// #include <iostream>     // std::cin, std::cout, std::cerr
// #include <string>       // std::string
// #include <sys/epoll.h>  // EPOLLIN, EPOLLOUT, EPOLLRDHUP
// #include <unistd.h>     // STDIN_FILENO
// #include <utility>      // std::move

// class Client {
// public:
//     Client(std::string server_ip, int port)
//         : server_ip_(std::move(server_ip)),
//           port_(port),
//           connection_(Socket()) {}

//     bool Start() {
//         signal(SIGPIPE, SIG_IGN);

//         Socket socket;

//         if (!socket.Create()) {
//             return false;
//         }

//         if (!socket.Connect(server_ip_, port_)) {
//             return false;
//         }

//         if (!socket.SetNonBlocking()) {
//             return false;
//         }

//         connection_ = TcpConnection(std::move(socket));

//         if (!epoll_.Create()) {
//             return false;
//         }

//         if (!epoll_.Add(STDIN_FILENO, EPOLLIN)) {
//             return false;
//         }

//         if (!epoll_.Add(connection_.Fd(), EPOLLIN | EPOLLRDHUP)) {
//             return false;
//         }

//         std::cout << "connected to server. type message:\n";
//         return true;
//     }

//     void Run() {
//         while (true) {
//             std::vector<EpollEvent> events = epoll_.Wait(-1);

//             for (const auto& event : events) {
//                 if (event.fd == STDIN_FILENO) {
//                     if (!HandleStdin()) {
//                         return;
//                     }

//                     continue;
//                 }

//                 if (event.fd == connection_.Fd()) {
//                     if (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
//                         std::cout << "server disconnected.\n";
//                         return;
//                     }

//                     if (event.events & EPOLLIN) {
//                         if (!HandleRead()) {
//                             return;
//                         }
//                     }

//                     if (event.events & EPOLLOUT) {
//                         if (!HandleWrite()) {
//                             return;
//                         }
//                     }
//                 }
//             }
//         }
//     }

// private:
//     bool HandleStdin() {
//         std::string input;
//         std::getline(std::cin, input);

//         if (!std::cin) {
//             return false;
//         }

//         if (input == "quit") {
//             std::cout << "quit client.\n";
//             return false;
//         }

//         if (!connection_.QueueMessage(input)) {
//             return false;
//         }

//         if (connection_.HasDataToWrite()) {
//             epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP);
//         }

//         return true;
//     }

//     bool HandleRead() {
//         if (!connection_.ReadFromSocket()) {
//             std::cout << "server closed connection.\n";
//             return false;
//         }

//         std::string message;

//         while (connection_.TryPopMessage(message)) {
//             std::cout << "server echo: " << message << '\n';
//         }

//         return true;
//     }

//     bool HandleWrite() {
//         if (!connection_.WriteToSocket()) {
//             return false;
//         }

//         if (connection_.HasDataToWrite()) {
//             epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP);
//         } else {
//             epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLRDHUP);
//         }

//         return true;
//     }

// private:
//     std::string server_ip_;
//     int port_;

//     Epoll epoll_;
//     TcpConnection connection_;
// };

// int main() {
//     Client client("127.0.0.1", 8080);

//     if (!client.Start()) {
//         return 1;
//     }

//     client.Run();

//     return 0;
// }
#include "../../include/client.h"

#include <csignal> // signal, SIGPIPE, SIG_IGN
#include <iostream>
#include <sys/epoll.h> // EPOLLIN, EPOLLOUT, EPOLLRDHUP
#include <unistd.h> // STDIN_FILENO
#include <utility> // std::move
#include <vector>

Client::Client(std::string server_ip, int port) 
: server_ip_(server_ip)
, port_(port)
, connection_(Socket())
{}



bool Client::Start() {

    // 忽略 SIGPIPE, 避免向已关闭连接send时进程被杀掉
    signal(SIGPIPE, SIG_IGN);
    Socket socket;
    // 创建客户端socket
    if (!socket.Create()) {
        return false;
    }
    // 连接服务器
    if (!socket.Connect(server_ip_, port_)) {
        return false;
    }
    // 设置非阻塞
    if (!socket.SetNonBlocking()) {
        return false;
    }
    // 连接成功的socket移动到TcpConnection中
    connection_ = TcpConnection(std::move(socket));

    // 创建epoll
    if (!epoll_.Create()) {
        return false;
    }
    //  监听标准输入
    if (!epoll_.Add(STDIN_FILENO, EPOLLIN)) {
        return false;
    }

    // 监听服务器socket
    if (!epoll_.Add(connection_.Fd(), EPOLLIN | EPOLLRDHUP)) {
        return false;
    }
    std::cout << "connected to server \n";
    return true;
}


void Client::Run() {
    while (true) {
        std::vector<EpollEvent> events = epoll_.Wait(-1);
        for (const auto& ev : events) {
            if (ev.fd == STDIN_FILENO) {
                if (!HandleStdin()) {
                    return;
                }
                continue;
            }
            // 服务器socket事件
            if (ev.fd == connection_.Fd()) {
                // 服务器断开或异常
                if (ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    std::cout << "server disconnected \n";
                    return;
                }
                // 服务器发来数据
                if (ev.events & EPOLLIN) {
                    if (!HandleRead()) {
                        return;
                    }
                }
                // socket可写，可以继续发送send_buffer中的数据
                if (ev.events & EPOLLOUT) {
                    if (!HandleWrite()) {
                        return;
                    }
                }
            }
        }
    }
}

// 处理键盘输入
bool Client::HandleStdin() {
    std::string input;
    std::getline(std::cin, input);

    if (!std::cin) {
        return false;
    }

    if (input == "quit") {
        std::cout << "quit client \n" ;
        return false;
    }
    // 把用户输入编码成[4字节长度头][正文]
    // 追加到send_buffer中
    if (!connection_.QueueMessage(input)) {
        return false;
    }
    if (connection_.HasDataToWrite()) {
        epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    }
    return true;
}

// 处理服务器发来的数据
bool Client::HandleRead() {
    if (!connection_.ReadFromSocket()) {
        std::cout << "server close connection \n";
        return false;
    }

    std::string message;
    while (connection_.TryPopMessage(message)) {
        std::cout << "server echo: " << message << '\n';
    }
    return true;
}

// 处理 socket 可写事件
bool Client::HandleWrite() {
    if (!connection_.WriteToSocket()) {
        return false;
    }

    if (!connection_.HasDataToWrite()) {
        epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    } else {
        epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLRDHUP);
    }
    return true;
}
