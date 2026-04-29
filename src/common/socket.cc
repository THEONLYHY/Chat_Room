#include "../../include/socket.h"

#include <arpa/inet.h>   // inet_pton, htons
#include <cerrno>        // errno
#include <cstring>       // strerror
#include <fcntl.h>       // fcntl

#include <iostream>      // std::cerr
#include <sys/socket.h>  // socket, setsockopt, bind, listen, accept, connect
#include <unistd.h>      // close

Socket::Socket()
    : fd_(-1) {}

Socket::Socket(int fd)
    : fd_(fd) {}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }

    return *this;
}

Socket::~Socket() {
    Close();
}

bool Socket::Create() {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        std::cerr << "socket failed: " << strerror(errno) << '\n';
        return false;
    }

    return true;
}

bool Socket::SetReuseAddr() {
    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "setsockopt SO_REUSEADDR failed: "
                  << strerror(errno) << '\n';
        return false;
    }

    return true;
}

bool Socket::SetNonBlocking() {
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "fcntl F_GETFL failed: "
                  << strerror(errno) << '\n';
        return false;
    }

    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "fcntl F_SETFL failed: "
                  << strerror(errno) << '\n';
        return false;
    }

    return true;
}
bool Socket::Bind(int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "bind failed: " << strerror(errno) << '\n';
        return false;
    }

    return true;
}

bool Socket::Listen(int backlog) {
    if (listen(fd_, backlog) == -1) {
        std::cerr << "listen failed: " << strerror(errno) << '\n';
        return false;
    }

    return true;
}

Socket Socket::Accept() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(fd_,
                           reinterpret_cast<sockaddr*>(&client_addr),
                           &client_len);
    if (client_fd == -1) {
        // 不能在这里打印错误
        // std::cerr << "accept failed: " << strerror(errno) << '\n';
        return Socket();
    }

    return Socket(client_fd);
}

bool Socket::Connect(const std::string& ip, int port) {
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    int ret = inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);
    if (ret == 0) {
        std::cerr << "invalid ip address: " << ip << '\n';
        return false;
    }

    if (ret == -1) {
        std::cerr << "inet_pton failed: " << strerror(errno) << '\n';
        return false;
    }

    if (connect(fd_, 
                reinterpret_cast<sockaddr*>(&server_addr), 
                sizeof(server_addr)) == -1) {
        std::cerr << "connect failed: " << strerror(errno) << '\n';
        return false;
    }

    return true;
}

ssize_t Socket::Send(const void* data, size_t length, int flags) {
    return send(fd_, data, length, flags);
}

ssize_t Socket::Recv(void* buffer, size_t length, int flags) {
    return recv(fd_, buffer, length, flags);
}

int Socket::Fd() const {
    return fd_;
}

bool Socket::IsValid() const {
    return fd_ != -1;
}

void Socket::Close() {
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}