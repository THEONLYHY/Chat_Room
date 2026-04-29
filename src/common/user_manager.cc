#include "../../include/user_manager.h"

UserManager::UserManager() {
    UserInfo root;
    root.password = "root123";
    root.online = false;
    root.fd = -1;
    root.root = true;
    users_["root"] = root;
}

bool UserManager::RegisterUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (username.empty() || password.empty()) {
        return false;
    }

    if (users_.find(username) != users_.end()) {
        return false;
    }

    users_[username] = UserInfo{password, false, -1, false};
    return true;
}

bool UserManager::DeleteUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto iter = users_.find(username);
    if (iter == users_.end()) {
        return false;
    }

    if (iter->second.root) {
        return false;
    }

    if (iter->second.online) {
        fd_to_username_.erase(iter->second.fd);
    }

    users_.erase(iter);
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
    return true;
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
    if (new_password.empty()) {
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

bool UserManager::IsRoot(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto iter = users_.find(username);
    if (iter == users_.end()) {
        return false;
    }

    return iter->second.root;
}

bool UserManager::UserExists(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return users_.find(username) != users_.end();
}

int UserManager::GetFdByUsername(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        return -1;
    }
    if (!it->second.online) {
        return -1;
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
        if (pair.second.online) {
            online_users.push_back(pair.first);
        }
    }
    return online_users;
}