#include "../../include/server.h"

#include <cerrno>       // errno
#include <cstring>      // strerror
#include <iostream>     // std::cout, std::cerr
#include <string>       // std::string
#include <sys/epoll.h>  // EPOLLIN, EPOLLOUT, EPOLLRDHUP
#include <unordered_map> // std::unordered_map
#include <utility>      // std::move


// class Server {
// public:
//     explicit Server(int port)
//         : port_(port) {}

//     bool Start() {
//         if (!listen_socket_.Create()) {
//             return false;
//         }

//         if (!listen_socket_.SetReuseAddr()) {
//             return false;
//         }

//         if (!listen_socket_.SetNonBlocking()) {
//             return false;
//         }

//         if (!listen_socket_.Bind(port_)) {
//             return false;
//         }

//         if (!listen_socket_.Listen(kBacklog)) {
//             return false;
//         }

//         if (!epoll_.Create()) {
//             return false;
//         }

//         if (!epoll_.Add(listen_socket_.Fd(), EPOLLIN)) {
//             return false;
//         }

//         std::cout << "server listen on port " << port_ << "...\n";
//         return true;
//     }

//     void Run() {
//         while (true) {
//             std::vector<EpollEvent> events = epoll_.Wait(-1);

//             for (const auto& event : events) {
//                 int fd = event.fd;

//                 if (fd == listen_socket_.Fd()) {
//                     HandleAccept();
//                     continue;
//                 }

//                 if (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
//                     RemoveConnection(fd);
//                     continue;
//                 }

//                 if (event.events & EPOLLIN) {
//                     if (!HandleRead(fd)) {
//                         continue;
//                     }
//                 }

//                 if (event.events & EPOLLOUT) {
//                     HandleWrite(fd);
//                 }
//             }
//         }
//     }

// private:
//     static constexpr int kBacklog = 128;

//     void HandleAccept() {
//         while (true) {
//             Socket client_socket = listen_socket_.Accept();

//             if (!client_socket.IsValid()) {
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     break;
//                 }

//                 if (errno == EINTR) {
//                     continue;
//                 }

//                 std::cerr << "accept failed: " << strerror(errno) << '\n';
//                 break;
//             }

//             if (!client_socket.SetNonBlocking()) {
//                 continue;
//             }

//             int client_fd = client_socket.Fd();

//             if (!epoll_.Add(client_fd, EPOLLIN | EPOLLRDHUP)) {
//                 continue;
//             }

//             connections_.emplace(client_fd,
//                                  TcpConnection(std::move(client_socket)));

//             std::cout << "client connected, fd = " << client_fd << '\n';
//         }
//     }

//     bool HandleRead(int fd) {
//         auto iter = connections_.find(fd);
//         if (iter == connections_.end()) {
//             return false;
//         }

//         TcpConnection& connection = iter->second;

//         if (!connection.ReadFromSocket()) {
//             RemoveConnection(fd);
//             return false;
//         }

//         std::string message;

//         while (connection.TryPopMessage(message)) {
//             std::cout << "client[" << fd << "]: "
//                       << message << '\n';

//             // Echo：客户端发什么，服务端原样返回什么
//             if (!connection.QueueMessage(message)) {
//                 RemoveConnection(fd);
//                 return false;
//             }
//         }

//         if (connection.HasDataToWrite()) {
//             epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
//         }

//         return true;
//     }

//     void HandleWrite(int fd) {
//         auto iter = connections_.find(fd);
//         if (iter == connections_.end()) {
//             return;
//         }

//         TcpConnection& connection = iter->second;

//         if (!connection.WriteToSocket()) {
//             RemoveConnection(fd);
//             return;
//         }

//         if (connection.HasDataToWrite()) {
//             epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
//         } else {
//             epoll_.Modify(fd, EPOLLIN | EPOLLRDHUP);
//         }
//     }

//     void RemoveConnection(int fd) {
//         epoll_.Remove(fd);
//         connections_.erase(fd);

//         std::cout << "connection removed, fd = " << fd << '\n';
//     }

// private:
//     int port_;
//     Socket listen_socket_;
//     Epoll epoll_;
//     std::unordered_map<int, TcpConnection> connections_;
// };

// int main() {
//     Server server(8080);

//     if (!server.Start()) {
//         return 1;
//     }

//     server.Run();

//     return 0;
// }


bool Server::Start() {
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
    std::cout << "server listen on port" << port_ << "...\n";
    return true;
}
void Server::Run() {
    while (true) {
        std::vector<EpollEvent> events = epoll_.Wait(-1);
        for (const auto& ev : events) {
            // 如果是监听的socket触发事件，说明有新的客户端连接
            if (ev.fd == listen_socket_.Fd()) {
                HandleAccept();
                continue;
            }
            // 如果客户端连接出错或断开，移除连接
            if (ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                Removeconncetion(ev.fd);
                continue;
            }
            // 客户端socket可读，说明客户端发来了数据
            if (ev.events & EPOLLIN) {
                if (!HandleRead(ev.fd)) {
                    continue;
                }
            }
            // 客户端socket可写，说明可以继续发送send_buffer_中的数据
            if (ev.events & EPOLLOUT) {
                HandleWrite(ev.fd);
            }
        }
    }
}

void Server::HandleAccept() {
    while (true) {
        // accept 
        Socket client_socket = listen_socket_.Accept();
        // accept返回-1
        if (!client_socket.IsValid()) {
            // accpet返回-1，但只是没有更多连接了
            // 非阻塞socket下， EAGAIN/EWOULDBLOCK 表示没有更多连接了
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            // 被信号中断，继续accept
            if (errno == EINTR) {
                continue;
            }
            // 这是真的accept返回失败
            std::cerr << "accept failed : " << strerror(errno) << '\n';
            break;
        }
        // 客户端socket设置成非阻塞
        if (!client_socket.SetNonBlocking()) {
            continue;
        }
        int client_fd = client_socket.Fd();
        
        // 把客户端socket加入epoll
        // EPOLLIN:监听客户端发数据
        // EPOLLRDHUP:监听客户端关闭连接/关闭写方向
        if (!epoll_.Add(client_fd, EPOLLIN | EPOLLRDHUP)) {
            continue;
        }
        // 添加到TcpConnection
        // TcpConnection负责该客户端的recv_buffer_，send_buffer_和粘包处理
        connections_.emplace(client_fd, TcpConnection(std::move(client_socket)));
        
        std::cout << "client conneted, fd = " << client_fd << '\n';
    }
}

bool Server::HandleRead(int fd) {
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
        return false;
    }
    TcpConnection& connection = iter->second;

    // 从socket中尽可能读数据到recv_buffer中
    if (!connection.ReadFromSocket()) {
        Removeconncetion(fd);
        return false;
    }
    std::string message;

    // 从recv_buffer中解析完整的信息
    while (connection.TryPopMessage(message)) {
        std::cout << "client[" << fd << "]: " << message << '\n';

        // Echo功能：客户端发什么，服务器就原样放入发送缓冲区
        if (!connection.QueueMessage(message)) {
            Removeconncetion(fd);
            return false;
        }
    }

    // 如果发送缓冲区里有数据，就打开EPOLLOUT
    // 等socket可写时再真正发送
    if (connection.HasDataToWrite()) {
        epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    }
    return true;
}

void Server::HandleWrite(int fd) {
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
        return;
    }
    TcpConnection& connection = iter->second;
    // 尽可能把send_buffer中的数据都发送出去
    if (!connection.WriteToSocket()) {
        Removeconncetion(fd);
        return;
    }

    //如果还有没发送完的数据，继续监听EPOLLOUT
    if (connection.HasDataToWrite()) {
        epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    } else {
        // 如果都发完了，取消EPOLLOUT
        // 避免socket一直可写导致epoll_wait频繁返回
        epoll_.Modify(fd, EPOLLIN | EPOLLRDHUP);
    }
}

void Server::Removeconncetion(int fd) {
    // 先从epoll删除
    epoll_.Remove(fd);

    // 再从connections_删除
    // TcpConnection 析构会触发Socket析构，自动close(fd)
    connections_.erase(fd);

    std::cout << "connection removed, fd = " << fd << '\n';
}