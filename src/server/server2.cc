#include "../../include/socket.h"
#include "../../include/epoll.h"

#include <iostream>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/epoll.h>
#include <unordered_map>
#include <utility>

int main() {
    Socket listen_socket;

    if (!listen_socket.Create()) {
        return 1;
    }

    if (!listen_socket.SetReuseAddr()) {
        return 1;
    }

    if (!listen_socket.Bind(8080)) {
        return 1;
    }

    if (!listen_socket.Listen(128)) {
        return 1;
    }

    std::cout << "server listen on port 8080...\n";

    // Socket client_socket = listen_socket.Accept();
    // if (!client_socket.IsValid()) {
    //     std::cerr << "accept failed\n";
    //     return 1;
    // }

    // std::cout << "client connected\n";

    // char buffer[1024] = {0};
    // ssize_t n = client_socket.Recv(buffer, sizeof(buffer) - 1);

    // if (n > 0) {
    //     std::cout << "recv: " << buffer << '\n';
    //     client_socket.Send(buffer, static_cast<size_t>(n));
    // }

    Epoll epoll;
    if (!epoll.Create()) {
        return 1;
    }

    if (!epoll.Add(listen_socket.Fd(), EPOLLIN)) {
        return 1;
    }

    std::unordered_map<int, Socket> clients;


    while (true) {
        std::vector<EpollEvent> events = epoll.Wait(-1);

        for (const auto& event : events) {
            int fd = event.fd;

            //处理新客户端连接
            if (fd == listen_socket.Fd()) {
                while (true) {
                    Socket client_socket = listen_socket.Accept();

                    if (!client_socket.IsValid()) {
                        // accept() 返回-1 并且errno = EAGAIN说明没有更多的连接了
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        if (errno == EINTR) {
                            continue;
                        }
                        std::cerr << "accept failed: " << strerror(errno) << '\n';
                        break;
                    }
                    // 设置成非阻塞
                    if (!client_socket.SetNonBlocking()) {
                        continue;
                    }
                    // 把fd加入epoll
                    int client_fd = client_socket.Fd();
                    if (!epoll.Add(client_fd, EPOLLIN | EPOLLRDHUP)) {
                        continue;
                    }
                    // 客户端保存到clients
                    clients.emplace(client_fd, std::move(client_socket));
                    std::cout << "client conneted fd = " << client_fd << '\n';
                }
                continue;
            }
            //EPOLLERR：连接出错
            //EPOLLHUP：连接挂起
            //EPOLLRDHUP：对端关闭连接
            if (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                epoll.Remove(fd);
                clients.erase(fd);
                std::cout << "client disconnected, fd = " << fd << '\n';
                continue;
            }
            // 有数据输入
            if (event.events & EPOLLIN) {
                auto iter = clients.find(fd);
                if (iter == clients.end()) {
                    continue;
                }

                char buffer[4096];
                Socket& client_socket = iter->second;
                while (true) {
                    ssize_t bytes_recv = client_socket.Recv(buffer, sizeof(buffer));
                    // 接收到数据
                    if (bytes_recv > 0) {
                        std::cout << "recv from fd"
                                  << fd << ": "
                                  << std::string(buffer, static_cast<size_t>(bytes_recv)) 
                                  << '\n';
                        client_socket.Send(buffer, static_cast<size_t>(bytes_recv));
                        continue;
                    }
                    // 对方关闭
                    if (bytes_recv == 0) {
                        epoll.Remove(fd);
                        clients.erase(fd);

                        std::cout << "client closed, fd = " << fd << '\n';
                        break;
                    }
                    // 没数据可读了
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    if (errno == EINTR) {
                        continue;
                    }
                    std::cerr << "recv failed: " << strerror(errno) << '\n';
                    epoll.Remove(fd);
                    clients.erase(fd);
                    break;
                }
            }
        }
    }
    return 0;
}