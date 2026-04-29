#ifndef INCLUDE_SERVER_H_
#define INCLUDE_SERVER_H_


#include "socket.h"
#include "epoll.h"
#include "tcp_connection.h"
#include "user_manager.h"

#include "nlohmann/json.hpp"

#include <unordered_map>
#include <string>


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
    bool HandleRead(int fd);
    void HandleWrite(int fd);
    void RemoveConncetion(int fd);

    // 根据消息类型进行消息分发
    void DispatchMessage(int fd, const nlohmann::json& message);
    bool RequireLogin(int fd);

    void HandleRegister(int fd, const nlohmann::json& message);
    void HandleLogin(int fd, const nlohmann::json& message);
    void HandleLogout(int fd);
    void HandleChangePassword(int fd, const nlohmann::json& message);
    void HandleOnlineUsers(int fd);
    void HandlePrivateChat(int fd, const nlohmann::json& message);
    void HandleGroupChat(int fd, const nlohmann::json& message);

    bool SendJson(int fd, const nlohmann::json& message);
    bool SendResponse(int fd, bool success, const std::string& reason);
    void BroadcastJson(const nlohmann::json& message, int except_fd = -1);

    int port_;
    Socket listen_socket_;
    Epoll epoll_;
    // key: fd , value: 连接
    std::unordered_map<int, TcpConnection> connections_;

    UserManager user_manager_;
};

#endif