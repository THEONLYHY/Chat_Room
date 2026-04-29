#include "client.h"

#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include <utility>

#include "logger.h"
#include "protocol.h"
#include "socket.h"

Client::Client(std::string server_ip, int port)
    : server_ip_(std::move(server_ip)),
      port_(port),
      connection_(Socket()) {}

bool Client::Start() {
    signal(SIGPIPE, SIG_IGN);

    Socket socket;
    if (!socket.Create()) {
        return false;
    }

    if (!socket.Connect(server_ip_, port_)) {
        return false;
    }

    if (!socket.SetNonBlocking()) {
        return false;
    }

    connection_ = TcpConnection(std::move(socket));

    if (!epoll_.Create()) {
        return false;
    }

    if (!epoll_.Add(STDIN_FILENO, EPOLLIN)) {
        return false;
    }

    if (!epoll_.Add(connection_.Fd(), EPOLLIN | EPOLLRDHUP)) {
        return false;
    }

    running_ = true;
    PrintHelp();
    return true;
}

void Client::Run() {
  while (running_) {
    std::vector<EpollEvent> events = epoll_.Wait(-1);

    for (const auto& event : events) {
        // 如果是标准输入有数据，用户输入了命令
        if (event.fd == STDIN_FILENO) {
            if (!HandleStdin()) {
                running_ = false;
                break;
            }
            continue;
        }
        // 如果是服务器连接对应的fd
        if (event.fd == connection_.Fd()) {
            // 连接异常、挂断、对端关闭
            if ((event.events & EPOLLERR) ||
                (event.events & EPOLLHUP) ||
                (event.events & EPOLLRDHUP)) {
                std::cout << "server disconnected\n";
                running_ = false;
                break;
            }
            // 服务器发来了数据
            if (event.events & EPOLLIN) {
                if (!HandleRead()) {
                    running_ = false;
                    break;
                }
            }
            // 可写，继续发送send_buffer中的数据
            if (event.events & EPOLLOUT) {
                if (!HandleWrite()) {
                    running_ = false;
                    break;
                }
            }
        }
    }
  }
}

bool Client::HandleStdin() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return false;
    }
    // 空行直接忽略
    if (line.empty()) {
        return true;
    }
    // 显示帮助命令
    if (line == "/help") {
        PrintHelp();
        return true;
    }
    // 退出命令
    // 先发一个logout给服务端，再结束客户端循环
    if (line == "/quit") {
        nlohmann::json logout;
        logout["type"] = "logout";
        SendJson(logout);
        // 主动写,尽量让logout立即发出
        connection_.WriteToSocket();
        return false;
    }
    // 其他命令解析成JSON
    nlohmann::json message;
    if (!ParseCommand(line, message)) {
        std::cout << "invalid command, input /help\n";
        return true;
    }
    // 把命令转成JSON后发送
    if (!SendJson(message)) {
        std::cout << "send command failed\n";
        return false;
    }

    return true;
}

bool Client::HandleRead() {
    // 把socket中的数据尽可能读到接收缓冲区
    if (!connection_.ReadFromSocket()) {
        return false;
    }

    std::string raw_message;
    // 不断从接收缓冲区解析完整消息
    while (connection_.TryPopMessage(raw_message)) {
        nlohmann::json message;
        if (!protocol::IsValidJson(raw_message, message)) {
            std::cout << "received invalid json from server\n";
            continue;
        }
        PrintServerMessage(message);
    }

    return true;
}

bool Client::HandleWrite() {
    if (!connection_.WriteToSocket()) {
        running_ = false;
        return false;
    }

    // 如果已经没有待发送数据了，就关闭 EPOLLOUT 监听
    // 避免因为socket总是可写，而让epoll频繁返回
    if (!connection_.HasDataToWrite()) {
        epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLRDHUP);
    }

    return true;
}

bool Client::SendJson(const nlohmann::json& message) {
    if (!connection_.QueueMessage(message.dump())) {
        return false;
    }
    if (!epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP)) {
        return false;
    }
    return epoll_.Modify(connection_.Fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP);
}

bool Client::ParseCommand(const std::string& line, nlohmann::json& message) {
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    //  注册命令： /resigter username password
    if (command == "/register") {
        std::string username;
        std::string password;
        iss >> username >> password;
        if (username.empty() || password.empty()) {
            return false;
        }
        message["type"] = "register";
        message["username"] = username;
        message["password"] = password;
        return true;
    }
    // 登录命令：/login username password
    if (command == "/login") {
        std::string username;
        std::string password;
        iss >> username >> password;
        if (username.empty() || password.empty()) {
            return false;
        }
        message["type"] = "login";
        message["username"] = username;
        message["password"] = password;
        return true;
    }
    // 登出命令： /logout
    if (command == "/logout") {
        message["type"] = "logout";
        return true;
    }
    // 修改密码命令： /passwd old new
    if (command == "/passwd") {
        std::string old_password;
        std::string new_password;
        iss >> old_password >> new_password;

        if (old_password.empty() || new_password.empty()) {
            return false;
        }
        message["type"] = "change_password";
        message["old_password"] = old_password;
        message["new_password"] = new_password;
        return true;
    }
    // 查询在线用户： /online
    if (command == "/online") {
        message["type"] = "online_users";
        return true;
    }

    // 私聊：/msg username content
    if (command == "/msg") {
        std::string to;
        iss >> to;

        std::string content;
        std::getline(iss, content);
        // 去掉开头多余的一个空格
        if (!content.empty() && content[0] == ' ') {
            content.erase(0, 1);
        }

        if (to.empty() || content.empty()) {
            return false;
        }

        message["type"] = "private_chat";
        message["to"] = to;
        message["content"] = content;
        return true;
    }
    // 群聊： /all content
    if (command == "/all") {
        std::string content;
        std::getline(iss, content);
        
        if (!content.empty() && content[0] == ' ') {
            content.erase(0, 1);
        }

        if (content.empty()) {
            return false;
        }

        message["type"] = "group_chat";
        message["content"] = content;
        return true;
    }

    return false;
}

void Client::PrintServerMessage(const nlohmann::json& message) {
    // 获取消息类型
    const std::string type = protocol::GetStringField(message, "type");
    // 通用响应消息
    if (type == "response") {
        const bool success = message.value("success", false);
        const std::string reason = protocol::GetStringField(message, "reason");

        std::cout << (success ? "[OK] " : "[FAILED] ") << reason;
        
        if (message.contains("users") && message["users"].is_array()) {
            std::cout << "\n[online users]";
            for (const auto& user : message["users"]) {
                if (user.is_string()) {
                    std::cout << " " << user.get<std::string>();
                }
            }
        }

        std::cout << '\n';
        return;
    }

    if (type == "system") {
        std::cout << "[system] "
                  << protocol::GetStringField(message, "content") << '\n';
        return;
    }

    if (type == "private_chat") {
        std::cout << "[private]["
                << protocol::GetStringField(message, "timestamp") << "]["
                << protocol::GetStringField(message, "from") << "] "
                << protocol::GetStringField(message, "content") << '\n';
        return;
    }

    if (type == "group_chat") {
        std::cout << "[group]["
                << protocol::GetStringField(message, "timestamp") << "]["
                << protocol::GetStringField(message, "from") << "] "
                << protocol::GetStringField(message, "content") << '\n';
        return;
    }

    std::cout << "[unknown] " << message.dump() << '\n';
}

void Client::PrintHelp() const {
  std::cout << "\ncommands:\n"
            << "  /register <username> <password>\n"
            << "  /login <username> <password>\n"
            << "  /logout\n"
            << "  /passwd <old_password> <new_password>\n"
            << "  /online\n"
            << "  /msg <username> <message>\n"
            << "  /all <message>\n"
            << "  /help\n"
            << "  /quit\n\n";
}
