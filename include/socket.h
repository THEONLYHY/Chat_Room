#ifndef SOCKET_H_
#define SOCKET_H_

#include <string>

class Socket {
public:
    Socket();
    explicit Socket(int fd);

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    ~Socket();

    bool Create();
    bool SetReuseAddr();
    bool SetNonBlocking();

    bool Bind(int port);
    bool Listen(int backlog);
    Socket Accept();
    bool Connect(const std::string& ip, int port);

    ssize_t Send(const void* data, size_t length, int flags = 0);
    ssize_t Recv(void* buffer, size_t length, int flags = 0);

    int Fd() const;
    bool IsValid() const;

    void Close();

private:
    int fd_;
};

#endif  // SOCKET_H_