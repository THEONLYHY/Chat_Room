#include "../../include/epoll.h"
#include "../../include/socket.h"
#include "../../include/tcp_connection.h"

#include <cerrno>       // errno
#include <cstring>      // strerror
#include <iostream>     // std::cout, std::cerr
#include <string>       // std::string
#include <sys/epoll.h>  // EPOLLIN, EPOLLOUT, EPOLLRDHUP
#include <unordered_map> // std::unordered_map
#include <utility>      // std::move

class Server {
public:
    explicit Server(int port)
        : port_(port) {}

    bool Start() {
        if (!listen_socket_.Create()) {
            return false;
        }

        if (!listen_socket_.SetReuseAddr()) {
            return false;
        }

        if (!listen_socket_.SetNonBlocking()) {
            return false;
        }

        if (!listen_socket_.Bind(port_)) {
            return false;
        }

        if (!listen_socket_.Listen(kBacklog)) {
            return false;
        }

        if (!epoll_.Create()) {
            return false;
        }

        if (!epoll_.Add(listen_socket_.Fd(), EPOLLIN)) {
            return false;
        }

        std::cout << "server listen on port " << port_ << "...\n";
        return true;
    }

    void Run() {
        while (true) {
            std::vector<EpollEvent> events = epoll_.Wait(-1);

            for (const auto& event : events) {
                int fd = event.fd;

                if (fd == listen_socket_.Fd()) {
                    HandleAccept();
                    continue;
                }

                if (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    RemoveConnection(fd);
                    continue;
                }

                if (event.events & EPOLLIN) {
                    if (!HandleRead(fd)) {
                        continue;
                    }
                }

                if (event.events & EPOLLOUT) {
                    HandleWrite(fd);
                }
            }
        }
    }

private:
    static constexpr int kBacklog = 128;

    void HandleAccept() {
        while (true) {
            Socket client_socket = listen_socket_.Accept();

            if (!client_socket.IsValid()) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                if (errno == EINTR) {
                    continue;
                }

                std::cerr << "accept failed: " << strerror(errno) << '\n';
                break;
            }

            if (!client_socket.SetNonBlocking()) {
                continue;
            }

            int client_fd = client_socket.Fd();

            if (!epoll_.Add(client_fd, EPOLLIN | EPOLLRDHUP)) {
                continue;
            }

            connections_.emplace(client_fd,
                                 TcpConnection(std::move(client_socket)));

            std::cout << "client connected, fd = " << client_fd << '\n';
        }
    }

    bool HandleRead(int fd) {
        auto iter = connections_.find(fd);
        if (iter == connections_.end()) {
            return false;
        }

        TcpConnection& connection = iter->second;

        if (!connection.ReadFromSocket()) {
            RemoveConnection(fd);
            return false;
        }

        std::string message;

        while (connection.TryPopMessage(message)) {
            std::cout << "client[" << fd << "]: "
                      << message << '\n';

            // Echo：客户端发什么，服务端原样返回什么
            if (!connection.QueueMessage(message)) {
                RemoveConnection(fd);
                return false;
            }
        }

        if (connection.HasDataToWrite()) {
            epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        }

        return true;
    }

    void HandleWrite(int fd) {
        auto iter = connections_.find(fd);
        if (iter == connections_.end()) {
            return;
        }

        TcpConnection& connection = iter->second;

        if (!connection.WriteToSocket()) {
            RemoveConnection(fd);
            return;
        }

        if (connection.HasDataToWrite()) {
            epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        } else {
            epoll_.Modify(fd, EPOLLIN | EPOLLRDHUP);
        }
    }

    void RemoveConnection(int fd) {
        epoll_.Remove(fd);
        connections_.erase(fd);

        std::cout << "connection removed, fd = " << fd << '\n';
    }

private:
    int port_;
    Socket listen_socket_;
    Epoll epoll_;
    std::unordered_map<int, TcpConnection> connections_;
};

int main() {
    Server server(8080);

    if (!server.Start()) {
        return 1;
    }

    server.Run();

    return 0;
}