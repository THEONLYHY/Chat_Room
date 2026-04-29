#ifndef INCLUDE_PROTOCOL_H_
#define INCLUDE_PROTOCOL_H_

#include <string>

#include "nlohmann/json.hpp"

namespace protocol {

enum class MessageType {
    kRegister,        // 注册请求
    kLogin,           // 登录请求
    kLogout,          // 退出登录请求
    kChangePassword,  // 修改密码请求
    kOnlineUsers,     // 获取在线用户列表请求
    kPrivateChat,     // 私聊消息
    kGroupChat,       // 群聊消息
    kDeleteUser,      // 删除用户
    kKickUser,        // 踢出在线用户
    kListUsers,       // 获取所有用户列表
    kSystem,          // 系统消息，例如服务器通知
    kResponse,        // 响应消息，例如操作成功 / 失败
    kUnknown          // 未知消息类型，用于处理非法 type
};

std::string NowTimeString();

// string -> enum MessageType
MessageType StringToMessageType(const std::string& type);
// MessageType -> string
std::string MessageTypeToString(MessageType type);

// 创建一个通用响应 JSON。
// 常用于服务器告诉客户端：某个操作成功还是失败。
// 例如：
// {
//   "type": "response",
//   "success": true,
//   "reason": "login success"
//   "timestamp": "2026-04-28 11:30:00"
// }
nlohmann::json MakeResponse(bool success, const std::string& reason);

// 创建一个系统消息 JSON。
// 常用于服务器主动通知客户端。
// 例如：
// {
//   "type": "system", 
//   "content": "user tom joined the chat room"
//   "timestamp": "2026-04-28 11:30:00"
// }
nlohmann::json MakeSystemMessage(const std::string& content);

// 创建一个聊天消息 JSON
// 
// 例如：
// {
//   "type": "private_chat",
//   "from": "aaa",
//   "content": "hello",
//   "timestamp": "2026-04-28 11:30:00"
// }
nlohmann::json MakeChatMessage(const std::string& type,
                               const std::string& from,
                               const std::string& content);
// online_users
nlohmann::json MakeOnlineUsersResponse(const std::vector<std::string>& users);
// all_users
nlohmann::json MakeAllUsersResponse(const std::vector<std::string>& users);
/**
 * @brief 
 * 检查字符串是否是合法JSON
 * @param data 
 * @param message 
 * @return true 解析成功，把解析后的JSON存入message
 * @return false 解析失败
 */
bool IsValidJson(const std::string& data, nlohmann::json& message);
/**
 * @brief Get the String Field object
 * 读取JSON字段，从JSON中取出某个key对用的字符串值
 * @param message 
 * @param key 
 * @return std::string 
 */
std::string GetStringField(const nlohmann::json& message, const std::string& key);

} // namespace protocol

#endif // INCLUDE_PROTOCOL_H