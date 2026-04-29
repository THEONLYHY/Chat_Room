#include "../../include/client.h"

#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include <utility>

#include "../../include/logger.h"
#include "../../include/protocol.h"
#include "../../include/socket.h"

Client::Client(std::string server_ip, int port)
    : server_ip_(std::move(server_ip)),
      port_(port),
      connection_(Socket()) {}

bool Client::Start() {
    signal(SIGPIPE, SIG_IGN);

    if (!Logger::Init()) {
        std::cerr << "logger init failed\n";
        return false;
    }

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
      if (event.fd == STDIN_FILENO) {
        if (!HandleStdin()) {
          running_ = false;
          break;
        }
        continue;
      }

      if (event.fd == connection_.Fd()) {
        if ((event.events & EPOLLERR) ||
            (event.events & EPOLLHUP) ||
            (event.events & EPOLLRDHUP)) {
          std::cout << "server disconnected\n";
          running_ = false;
          break;
        }

        if (event.events & EPOLLIN) {
          if (!HandleRead()) {
            running_ = false;
            break;
          }
        }

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

    if (line.empty()) {
        return true;
    }

    if (line == "/help") {
        PrintHelp();
        return true;
    }

    if (line == "/quit") {
        nlohmann::json logout;
        logout["type"] = "logout";
        SendJson(logout);
        connection_.WriteToSocket();
        return false;
    }

    nlohmann::json message;
    if (!ParseCommand(line, message)) {
        std::cout << "invalid command, input /help\n";
        return true;
    }

    if (!SendJson(message)) {
        std::cout << "send command failed\n";
        return false;
    }

    return true;
}

bool Client::HandleRead() {
    if (!connection_.ReadFromSocket()) {
        return false;
    }

    std::string raw_message;
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

    if (command == "/logout") {
        message["type"] = "logout";
        return true;
    }

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

    if (command == "/online") {
        message["type"] = "online_users";
        return true;
    }

    if (command == "/msg") {
        std::string to;
        iss >> to;

        std::string content;
        std::getline(iss, content);
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
    const std::string type = protocol::GetStringField(message, "type");

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
