#ifndef INCLUDE_CLIENT_H_
#define INCLUDE_CLIENT_H_


#include "epoll.h"
#include "tcp_connection.h"
#include <string>

/**
 * @brief 
 * 1.连接服务器
 * 2. 使用epoll同时监听stdin和服务器socket
 * 3. 用户输入消息后发送给服务器
 * 4. 接受服务器echo回来的消息
 * 
 */
class Client {
public:
    Client(std::string server_ip, int port);

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /// @brief 初始化客户端：socket->connect->nonblocking->epoll add stdin/
    /// @return 
    bool Start();

    /// @brief 进入epoll事件循环
    void Run();
private:
    /// @brief 处理键盘输入
    /// @return 
    bool HandleStdin();

    /// @brief 处理服务器发送来的数据
    /// @return 
    bool HandleRead();

    /// @brief 处理socket可写事件
    /// @return 
    bool HandleWrite();
    std::string server_ip_;
    int port_;

    Epoll epoll_; // 同时监听stdin和socket
    TcpConnection connection_;
};
#endif