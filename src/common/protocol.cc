#include "../../include/protocol.h"

namespace protocol {

MessageType StringToMessageType(const std::string& type) {
  if (type == "register") {
    return MessageType::kRegister;
  }

  if (type == "login") {
    return MessageType::kLogin;
  }

  if (type == "logout") {
    return MessageType::kLogout;
  }

  if (type == "change_password") {
    return MessageType::kChangePassword;
  }

  if (type == "online_users") {
    return MessageType::kOnlineUsers;
  }

  if (type == "private_chat") {
    return MessageType::kPrivateChat;
  }

  if (type == "group_chat") {
    return MessageType::kGroupChat;
  }

  if (type == "system") {
    return MessageType::kSystem;
  }

  if (type == "response") {
    return MessageType::kResponse;
  }

  // 如果客户端传来了无法识别的 type，就返回 unknown。
  // 服务器可以根据 kUnknown 判断这是非法请求。
  return MessageType::kUnknown;
}

// 把 C++ 枚举转换成 JSON 中使用的字符串。
// 这个函数通常用于构造 JSON 消息。
std::string MessageTypeToString(MessageType type) {
  switch (type) {
    case MessageType::kRegister:
      return "register";

    case MessageType::kLogin:
      return "login";

    case MessageType::kLogout:
      return "logout";

    case MessageType::kChangePassword:
      return "change_password";

    case MessageType::kOnlineUsers:
      return "online_users";

    case MessageType::kPrivateChat:
      return "private_chat";

    case MessageType::kGroupChat:
      return "group_chat";

    case MessageType::kSystem:
      return "system";

    case MessageType::kResponse:
      return "response";

    default:
      return "unknown";
  }
}

std::string NowTimeString() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
    localtime_r(&now_time, &local_time);

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

nlohmann::json MakeResponse(bool success, const std::string& reason) {
    nlohmann::json response;

    // type 字段表示这是一条响应消息。
    response["type"] = "response";

    // success 字段表示请求处理结果。
    response["success"] = success;

    // reason 字段表示原因，方便客户端显示提示信息。
    response["reason"] = reason;

    response["timestamp"] = NowTimeString();
    return response;
}

nlohmann::json MakeSystemMessage(const std::string& content) {
    nlohmann::json message;

    // type 字段表示这是一条系统消息。
    message["type"] = "system";

    // content 字段表示系统通知内容。
    message["content"] = content;

    message["timestamp"] = NowTimeString();
    return message;
}

nlohmann::json MakeChatMessage(const std::string& type,
                               const std::string& from,
                               const std::string& content) {
    nlohmann::json message;
    message["type"] = type;
    message["from"] = from;
    message["content"] = content;
    message["timestamp"] = NowTimeString();
    return message;
}

nlohmann::json MakeOnlineUsersResponse(const std::vector<std::string>& users) {
    nlohmann::json response = MakeResponse(true, "online users");
    response["users"] = users;
    return response;
}

bool IsValidJson(const std::string& data, nlohmann::json& message) {
    try {
        // 如果data能解析就合法，不合法抛异常
        message = nlohmann::json::parse(data);
        // 解析成功
        return true;
    } catch (...) {
        return false;
    }
    return message.is_object();
}

std::string GetStringField(const nlohmann::json& message,
                           const std::string& key) {
  if (!message.contains(key) || !message.at(key).is_string()) {
    return "";
  }
  return message.at(key).get<std::string>();
}

}
