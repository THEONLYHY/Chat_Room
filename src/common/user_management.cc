#include "echo_chat_basic/include/user_management.h"

bool UserManager::RegisterUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (username.empty() || password.empty()) {
        return false;
    }

    if (users_.find(username) != users_.end()) {
        return false;
    }

    users_[username] = UserInfo{password, false, -1};
    return true;
}

bool UserManager::Login(const std::string& username, const std::string& password, int fd) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    UserInfo& user = it->second;
    if (user.password != password) {
        return false;
    }
    if (user.online) {
        return false;
    }
    user.online = true;
    user.fd = fd;
    fd_to_username_[fd] = username;
}

bool UserManager::LogoutByFd(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fd_to_username_.find(fd);
    if (it == fd_to_username_.end()) {
        return false;
    }
    const std::string username = it->second;
    fd_to_username_.erase(it);

    auto user_it = users_.find(username);
    if (user_it != users_.end()) {
        user_it->second.online = false;
        user_it->second.fd = -1;
    }
    return true;
}

bool UserManager::ChangePassword(const std::string& username,
                    const std::string& old_password,
                    const std::string& new_password) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }

    if (it->second.password != old_password) {
        return false;
    }
    it->second.password = new_password;
    return true;
}

bool UserManager::IsOnline(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    return it->second.online;
}

int UserManager::GetFdByUsername(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    return it->second.fd;
}

std::string UserManager::GetUsernameByFd(int fd) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = fd_to_username_.find(fd);
    if (it == fd_to_username_.end()) {
        return "";
    }
    return it->second;
}

std::vector<std::string> UserManager::GetOnlineUsers() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> online_users;
    for (const auto& pair : users_) {
        const std::string& username = pair.first;
        const UserInfo& user = pair.second;

        if (user.online) {
            online_users.push_back(username);
        }
    }
    return online_users;
}