#include "../../include/tcp_connection.h"

#include <arpa/inet.h>   // htonl, ntohl
#include <cerrno>        // errno
#include <cstdint>       // uint32_t
#include <cstring>       // memcpy, strerror
#include <iostream>      // std::cerr, std::cout
#include <limits>        // std::numeric_limits
#include <sys/socket.h>  // MSG_NOSIGNAL
#include <utility>       // std::move

TcpConnection::TcpConnection(Socket socket) : socket_(std::move(socket)) {}


int TcpConnection::Fd() const {
    return socket_.Fd();
}

bool TcpConnection::IsValid() const {
    return socket_.IsValid();
}

bool TcpConnection::ReadFromSocket() {
    char buffer[4096];
    while (true) {
        ssize_t bytes_recv = socket_.Recv(buffer, sizeof(buffer));

        if (bytes_recv > 0) {
            recv_buffer_.append(buffer, static_cast<size_t>(bytes_recv));
            continue;
        }

        if (bytes_recv == 0) {
            std::cout << "peer closed connection, fd = " << Fd() << '\n';
            return false;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }

        std::cerr << "recv failed, fd = "
                  << Fd() << ", error = "
                  << strerror(errno) << '\n';
        return false;
    }
}
bool TcpConnection::WriteToSocket() {
    while (!send_buffer_.empty()) {
        // 如果对方已经关闭连接，你还继续send(),Linux可能会发送SIGPIPE, SIGPIPE会导致退出
        // 不要因为这个 send 产生 SIGPIPE 信号。
        ssize_t bytes_send = socket_.Send(send_buffer_.data(),
                                          send_buffer_.size(), 
                                          MSG_NOSIGNAL);
        // 发送成功一部分                                 
        if (bytes_send > 0) {
            send_buffer_.erase(0, static_cast<size_t>(bytes_send));
            continue;
        }
        
        if (bytes_send == 0) {
            return true;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }

        std::cerr << "send failed, fd = "
                  << Fd() << ", error = "
                  << strerror(errno) << '\n';
        return false;
    }
    return true;
}

bool TcpConnection::TryPopMessage(std::string& message) {
    message.clear();

    // 1. 连包头都不够，不能解析
    if (recv_buffer_.size() < kHeaderSize) {
        return false;
    }

    // 2. 取出前 4 个字节，得到网络字节序的 body 长度
    uint32_t network_length = 0;
    std::memcpy(&network_length, recv_buffer_.data(), kHeaderSize);

    // 3. 转成本机字节序
    uint32_t body_length = ntohl(network_length);
    
    // 4. 防止异常大包
    if (body_length > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        std::cerr << "message too large, fd = " << Fd() << '\n';
        return false;
    }
    
    // 5. 判断完整消息是否已经收到
    const size_t total_length = kHeaderSize + static_cast<size_t>(body_length);
    
    if (recv_buffer_.size() < total_length) {
        return false;
    }

    // 6. 从缓冲区取出正文部分
    message.assign(recv_buffer_.data() + kHeaderSize, body_length);

    // 7. 从接收缓冲区删除已经取出的完整消息
    recv_buffer_.erase(0, total_length);
    return true;
}

bool TcpConnection::QueueMessage(const std::string& message) {
    if (message.size() > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "message too large\n";
        return false;
    }

    uint32_t body_length = static_cast<uint32_t>(message.size());
    uint32_t network_length = htonl(body_length);

    const char* header = reinterpret_cast<const char*>(&network_length);

    send_buffer_.append(header, kHeaderSize);
    send_buffer_.append(message);

    return true;
}

bool TcpConnection::HasDataToWrite() const {
    return !send_buffer_.empty();
}


void TcpConnection::Close() {
    socket_.Close();
}