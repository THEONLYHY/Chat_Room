#include "../../include/server.h"
#include "../../include/logger.h"
#include "../../include/protocol.h"

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

    if (!Logger::Init()) {
        return false;
    }

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
        for (const auto& ev : events) {
            if (ev.fd == listen_socket_.Fd()) {
                HandleAccept();
                continue;
            }
            // 如果客户端连接出错或断开，移除连接
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

        if (!protocol::IsValidJson(raw_message, message)) {
            SendResponse(fd, false, "invalid json");
            continue;
        }

        DispatchMessage(fd, message);
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
        LOG_WARN("write event for unknown fd = {}", fd);
        return;
    }
    TcpConnection& connection = iter->second;
    // 尽可能把send_buffer中的数据都发送出去
    if (!connection.WriteToSocket()) {
        RemoveConncetion(fd);
        return;
    }

    //如果还有没发送完的数据，继续监听EPOLLOUT
    if (!connection.HasDataToWrite()) {
        // 如果都发完了，取消EPOLLOUT
        // 避免socket一直可写导致epoll_wait频繁返回
        epoll_.Modify(fd, EPOLLIN | EPOLLRDHUP);
    }
}

void Server::RemoveConncetion(int fd) {
    std::string username = user_manager_.GetUsernameByFd(fd);

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
    const std::string type = protocol::GetStringField(message, "type");
    const protocol::MessageType message_type = protocol::StringToMessageType(type);

    const bool logged_in = !user_manager_.GetUsernameByFd(fd).empty();

    if (!logged_in && message_type != protocol::MessageType::kRegister &&
        message_type != protocol::MessageType::kLogin) {
        SendResponse(fd, false, "please login first");
        return;
    }

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
    if (user_manager_.GetUsernameByFd(fd).empty()) {
        SendResponse(fd, false, "please login first");
        return false;
    }
    return true;
}

void Server::HandleRegister(int fd, const nlohmann::json& message) {
    const std::string username = protocol::GetStringField(message, "username");
    const std::string password = protocol::GetStringField(message, "password");

    if (username.empty() || password.empty()) {
        SendResponse(fd, false, "username or password is empty");
        return;
    }

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

    SendResponse(fd, true, "login success");
    BroadcastJson(protocol::MakeSystemMessage(username + " online"), fd);
    LOG_INFO("login success, username = {}, fd = {}", username, fd);
}

void Server::HandleLogout(int fd) {
    if (!RequireLogin(fd)) {
        return;
    }

    const std::string username = user_manager_.GetUsernameByFd(fd);
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

    SendJson(fd, protocol::MakeOnlineUsersResponse(user_manager_.GetOnlineUsers()));
}

void Server::HandlePrivateChat(int fd, const nlohmann::json& message) {
    if (!RequireLogin(fd)) {
        return;
    }

    const std::string from = user_manager_.GetUsernameByFd(fd);
    const std::string to = protocol::GetStringField(message, "to");
    const std::string content = protocol::GetStringField(message, "content");

    if (to.empty() || content.empty()) {
        SendResponse(fd, false, "target user or content is empty");
        return;
    }

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
    if (!connection.QueueMessage(message.dump())) {
        return false;
    }

    return epoll_.Modify(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
}

bool Server::SendResponse(int fd, bool success, const std::string& reason) {
    return SendJson(fd, protocol::MakeResponse(success, reason));
}

void Server::BroadcastJson(const nlohmann::json& message, int except_fd) {
    std::vector<std::string> users = user_manager_.GetOnlineUsers();
    for (const std::string& username : users) {
        const int target_fd = user_manager_.GetFdByUsername(username);
        if (target_fd != -1 && target_fd != except_fd) {
        SendJson(target_fd, message);
        }
    }
}
