#ifndef INCLUDE_USER_MANAGER_H_
#define INCLUDE_USER_MANAGER_H_

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class UserManager {
public:
    UserManager();
    /**
     * @brief 注册新用户
     * 
     * @param username 
     * @param password 
     * @return true 
     * @return false 
     */
    bool RegisterUser(const std::string& username, const std::string& password);
    bool DeleteUser(const std::string& username);
    /**
     * @brief 用户登录
     * 失败：
     * 1.用户不存在 2.密码错误 3.用户已经在线
     * @param username 
     * @param password 
     * @param fd 
     * @return true 
     * @return false 
     */
    bool Login(const std::string& username, const std::string& password, int fd);
    bool LogoutByFd(int fd);
    bool ChangePassword(const std::string& username,
                        const std::string& old_password,
                        const std::string& new_password);

    bool IsOnline(const std::string& username) const;
    bool IsRoot(const std::string& username) const;
    bool UserExists(const std::string& username) const;
    /**
     * @brief Get the Fd By Username objectd
     * 
     * @param username 
     * @return int 
     */
    int GetFdByUsername(const std::string& username) const;
    
    /**
     * @brief Get the Username By Fd object
     * 
     * @param fd 
     * @return std::string 
     */
    std::string GetUsernameByFd(int fd) const;

    /**
     * @brief Get the Online Users object
     * 
     * @return std::vector<std::string> 
     */
    std::vector<std::string> GetOnlineUsers() const;
private:
    struct UserInfo {
        std::string password; //用户密码
        bool online = false; // 用户是否在线
        int fd = -1; // 用户对应的socket fd
        bool root = false;
    };
    
    // mutable:即使在const成员函数中，如IsOnline(), GetOnlineUsers()
    // 也可以对mutex_加锁。
    mutable std::mutex mutex_;
    // key: 用户名 value: 用户信息
    std::unordered_map<std::string, UserInfo> users_;
    // key: socket fd, value: username
    // 主要用于客户端断开连接时，通过 fd 快速找到对应用户。
    std::unordered_map<int, std::string> fd_to_username_;
};

#endif // INCLUDE_USER_MANAGER_H_