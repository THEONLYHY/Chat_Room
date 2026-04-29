#include "server.h"
#include "logger.h"
#include "protocol.h"

#include <csignal>
#include <cerrno>       // errno
#include <cstring>      // strerror
#include <iostream>     // std::cout, std::cerr
#include <string>       // std::string
#include <sys/epoll.h>  // EPOLLIN, EPOLLOUT, EPOLLRDHUP
#include <unordered_map> // std::unordered_map
#include <utility>      // std::move
#include <vector>

using json = nlohmann::json;

bool Server::Start() {
    signal(SIGPIPE, SIG_IGN);

    if (!listen_socket_.Create()) {
        return false;
    }

    if (!listen_socket_.SetReuseAddr()) {
        return false;
    }

    if (!listen_socket_.Bind(port_)) {
        return false;
    }

    if (!listen_socket_.Listen(kBacklog)) {
        return false;
    }

    if (!listen_socket_.SetNonBlocking()) {
        return false;
    }

    if (!epoll_.Create()) {
        return false;
    }

    if (!epoll_.Add(listen_socket_.Fd(), EPOLLIN)) {
        return false;
    }

    LOG_INFO("server started, port = {}", port_);
    return true;
}

void Server::Run() {
    while (true) {
        std::vector<EpollEvent> events = epoll_.Wait(-1);
        // 循环助理本轮返回的事件
        for (const auto& ev : events) {
            // 有新客户端连接
            if (ev.fd == listen_socket_.Fd()) {
                HandleAccept();
                continue;
            }
            // 客户端连接异常、挂断、关闭写端，移除连接
            if (ev.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                RemoveConncetion(ev.fd);
                continue;
            }
            // 客户端socket可读，说明客户端发来了数据
            if (ev.events & EPOLLIN) {
                if (!HandleRead(ev.fd)) {
                    RemoveConncetion(ev.fd);
                    continue;
                }
            }
            // 客户端socket可写，发送缓冲区send_buffer_数据
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
            LOG_ERROR("accept failed: {}", strerror(errno));
            break;
        }
        int client_fd = client_socket.Fd();

        // 客户端socket设置成非阻塞
        if (!client_socket.SetNonBlocking()) {
            LOG_ERROR("set client socket nonblocking failed, fd = {}", client_fd);
            client_socket.Close();
            continue;
        }
        // 把客户端socket加入epoll
        // EPOLLIN:监听客户端发数据
        // EPOLLRDHUP:监听客户端关闭连接/关闭写方向
        if (!epoll_.Add(client_fd, EPOLLIN | EPOLLRDHUP)) {
            LOG_ERROR("add client fd to epoll failed, fd = {}", client_fd);
            client_socket.Close();
            continue;
        }
        // 添加到TcpConnection
        // TcpConnection负责该客户端的recv_buffer_，send_buffer_和粘包处理
        connections_.emplace(client_fd, TcpConnection(std::move(client_socket)));
        
        LOG_INFO("client connected, fd = {}", client_fd);
    }
}

bool Server::HandleRead(int fd) {
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
        LOG_WARN("read event for unknown fd = {}", fd);
        return false;
    }

    TcpConnection& connection = iter->second;

    // 从socket中尽可能读数据到recv_buffer中
    if (!connection.ReadFromSocket()) {
        return false;
    }

    std::string raw_message;

    // 从recv_buffer中解析完整的信息
    while (connection.TryPopMessage(raw_message)) {
        json message;
        // 解析JSON失败，则回复错误，但不关闭连接
        if (!protocol::IsValidJson(raw_message, message)) {
            SendResponse(fd, false, "invalid json");
            continue;
        }
        // JSON合法， 分发消息
        DispatchMessage(fd, message);
    }

    // 如果发送缓冲区里还有数据，就打开EPOLLOUT
    // 等socket可写时再真正发送，避免阻塞
    if (connection.HasDataToWrite()) {
        epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    }
    return true;
}

void Server::HandleWrite(int fd) {
    // 找到fd对应的连接
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
        LOG_WARN("write event for unknown fd = {}", fd);
        return;
    }
    TcpConnection& connection = iter->second;
    // 尽可能把send_buffer中的数据都发送出去
    if (!connection.WriteToSocket()) {
        RemoveConncetion(fd);
        return;
    }

    // 如果都发完了，关闭EPOLLOUT监听
    // 避免socket一直可写导致epoll_wait频繁返回
    if (!connection.HasDataToWrite()) {
        epoll_.Modify(fd, EPOLLIN | EPOLLRDHUP);
    }
}

void Server::RemoveConncetion(int fd) {
    // 根据fd查用户名
    std::string username = user_manager_.GetUsernameByFd(fd);

    // 如果对应连接的是已登录用户，先登出并广播下线消息
    if (!username.empty()) {
        user_manager_.LogoutByFd(fd);
        BroadcastJson(protocol::MakeSystemMessage(username + " offline "), fd);
        LOG_INFO("user logout by disconnect, username = {}, fd = {}", username, fd);
    }
    // 先从epoll删除
    epoll_.Remove(fd);

    // 再从connections_删除
    // TcpConnection 析构会触发Socket析构，自动close(fd)
    connections_.erase(fd);
    LOG_INFO("connection removed, fd = {}", fd);
}


// 消息分发
void Server::DispatchMessage(int fd, const nlohmann::json& message) {
    // 取出type字段， 获得消息类型
    const std::string type = protocol::GetStringField(message, "type");
    const protocol::MessageType message_type = protocol::StringToMessageType(type);

    const std::string username = user_manager_.GetUsernameByFd(fd);
    const bool logged_in = user_manager_.IsOnline(username);
    // 未登录的情况，只允许register和login
    if (!logged_in && message_type != protocol::MessageType::kRegister &&
        message_type != protocol::MessageType::kLogin) {
        SendResponse(fd, false, "please login first");
        return;
    }
    // 根据消息类型进行不同的业务处理
    switch (message_type) {
        case protocol::MessageType::kRegister:
            HandleRegister(fd, message);
            break;
        case protocol::MessageType::kLogin:
            HandleLogin(fd, message);
            break;
        case protocol::MessageType::kLogout:
            HandleLogout(fd);
            break;
        case protocol::MessageType::kChangePassword:
            HandleChangePassword(fd, message);
            break;
        case protocol::MessageType::kOnlineUsers:
            HandleOnlineUsers(fd);
            break;
        case protocol::MessageType::kPrivateChat:
            HandlePrivateChat(fd, message);
            break;
        case protocol::MessageType::kGroupChat:
            HandleGroupChat(fd, message);
            break;
        default:
            SendResponse(fd, false, "unknown message type");
            break;
    }
}

bool Server::RequireLogin(int fd) {
    // 当前fd是否登录
    const std::string username = user_manager_.GetUsernameByFd(fd);
    const bool logged_in = user_manager_.IsOnline(username);
    if (!logged_in) {
        SendResponse(fd, false, "please login first");
        return false;
    }
    return true;
}

void Server::HandleRegister(int fd, const nlohmann::json& message) {
    // 取出用户名和密码
    const std::string username = protocol::GetStringField(message, "username");
    const std::string password = protocol::GetStringField(message, "password");

    if (username.empty() || password.empty()) {
        SendResponse(fd, false, "username or password is empty");
        return;
    }
    // 注册失败，说明用户名存在或注册未通过
    if (!user_manager_.RegisterUser(username, password)) {
        SendResponse(fd, false, "username already exists");
        return;
    }

    SendResponse(fd, true, "register success");
    LOG_INFO("register success, username = {}", username);
}

void Server::HandleLogin(int fd, const nlohmann::json& message) {
    const std::string username = protocol::GetStringField(message, "username");
    const std::string password = protocol::GetStringField(message, "password");

    if (username.empty() || password.empty()) {
        SendResponse(fd, false, "username or password is empty");
        return;
    }

    if (!user_manager_.Login(username, password, fd)) {
        SendResponse(fd, false, "login failed");
        return;
    }

    // 登录成功，回复客户端并广播系统消息
    SendResponse(fd, true, "login success");
    BroadcastJson(protocol::MakeSystemMessage(username + " online"), fd);
    LOG_INFO("login success, username = {}, fd = {}", username, fd);
}

void Server::HandleLogout(int fd) {
    // 必须先登录
    if (!RequireLogin(fd)) {
        return;
    }

    const std::string username = user_manager_.GetUsernameByFd(fd);
    // 从用户管理器中登出
    user_manager_.LogoutByFd(fd);

    SendResponse(fd, true, "logout success");
    BroadcastJson(protocol::MakeSystemMessage(username + " offline"), fd);
    LOG_INFO("logout success, username = {}, fd = {}", username, fd);
}

void Server::HandleChangePassword(int fd, const nlohmann::json& message) {
    if (!RequireLogin(fd)) {
        return;
    }

    const std::string username = user_manager_.GetUsernameByFd(fd);
    const std::string old_password = protocol::GetStringField(message, "old_password");
    const std::string new_password = protocol::GetStringField(message, "new_password");

    if (old_password.empty() || new_password.empty()) {
        SendResponse(fd, false, "old password or new password is empty");
        return;
    }

    if (!user_manager_.ChangePassword(username, old_password, new_password)) {
        SendResponse(fd, false, "change password failed");
        return;
    }

    SendResponse(fd, true, "change password success");
    LOG_INFO("change password success, username = {}", username);
}

void Server::HandleOnlineUsers(int fd) {
    if (!RequireLogin(fd)) {
        return;
    }
    // 返回在线用户列表
    SendJson(fd, protocol::MakeOnlineUsersResponse(user_manager_.GetOnlineUsers()));
}

void Server::HandlePrivateChat(int fd, const nlohmann::json& message) {
    if (!RequireLogin(fd)) {
        return;
    }

    // 发送者
    const std::string from = user_manager_.GetUsernameByFd(fd);
    // 目标者
    const std::string to = protocol::GetStringField(message, "to");
    const std::string content = protocol::GetStringField(message, "content");

    if (to.empty() || content.empty()) {
        SendResponse(fd, false, "target user or content is empty");
        return;
    }
    // 目标用户在线fd
    const int target_fd = user_manager_.GetFdByUsername(to);
    if (target_fd == -1) {
        SendResponse(fd, false, "target user is not online");
        return;
    }

    SendJson(target_fd, protocol::MakeChatMessage("private_chat", from, content));
    SendResponse(fd, true, "private message sent");
    LOG_INFO("private chat, from = {}, to = {}", from, to);
}

void Server::HandleGroupChat(int fd, const nlohmann::json& message) {
    if (!RequireLogin(fd)) {
        return;
    }

    const std::string from = user_manager_.GetUsernameByFd(fd);
    const std::string content = protocol::GetStringField(message, "content");

    if (content.empty()) {
        SendResponse(fd, false, "content is empty");
        return;
    }

    BroadcastJson(protocol::MakeChatMessage("group_chat", from, content), fd);
    SendResponse(fd, true, "group message sent");
    LOG_INFO("group chat, from = {}", from);
}

bool Server::SendJson(int fd, const nlohmann::json& message) {
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
        return false;
    }

    TcpConnection& connection = iter->second;
    // 把JSON字符串放进发送缓冲区
    if (!connection.QueueMessage(message.dump())) {
        return false;
    }
    // 打开EPOLLOUT， 等待发送
    return epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
}

// response封装
bool Server::SendResponse(int fd, bool success, const std::string& reason) {
    return SendJson(fd, protocol::MakeResponse(success, reason));
}

void Server::BroadcastJson(const nlohmann::json& message, int except_fd) {
    // 获得当前所有在线用户
    std::vector<std::string> users = user_manager_.GetOnlineUsers();
    for (const std::string& username : users) {
        const int target_fd = user_manager_.GetFdByUsername(username);
        // 跳过无效fd和除外的fd
        if (target_fd != -1 && target_fd != except_fd) {
            SendJson(target_fd, message);
        }
    }
}
