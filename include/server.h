#ifndef INCLUDE_SERVER_H_
#define INCLUDE_SERVER_H_


#include "socket.h"
#include "epoll.h"
#include "tcp_connection.h"
#include <unordered_map>


class Server {
public:
    explicit Server(int port) : port_(port) {}

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * @brief 创建fd,绑定，监听，并把fd添加到epfd
     * 
     * @return true 
     * @return false 
     */
    bool Start();
    /**
     * @brief 循环遍历事件，进行处理
     * 
     */
    void Run();
private:
    static constexpr int kBacklog = 128;

    void HandleAccept();
    /**
     * @brief 处理客户端的可读事件
     * 
     * @param fd 
     * @return true 
     * @return false 
     */
    bool HandleRead(int fd);
    /**
     * @brief 处理客户端的可写事件
     * 
     * @param fd 
     */
    void HandleWrite(int fd);
    void Removeconncetion(int fd);

    int port_;
    Socket listen_socket_;
    Epoll epoll_;
    std::unordered_map<int, TcpConnection> connections_;
};

#endif