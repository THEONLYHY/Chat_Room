#ifndef INCLUDE_TCP_CONNECTION_H_
#define INCLUDE_TCP_CONNECTION_H_

#include "socket.h"

#include <cstddef>  // size_t
#include <string>   // std::string
#include <cstdint> // uint32_t

class TcpConnection {
public:
    explicit TcpConnection(Socket socket);

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    TcpConnection(TcpConnection&&) noexcept = default;
    TcpConnection& operator=(TcpConnection&&) noexcept = default;

    int Fd() const;
    bool IsValid() const;

    /**
     * @brief 读数据到recv_buffer中
     * 
     * @return true 
     * @return false 
     */
    bool ReadFromSocket();
    bool WriteToSocket();

    // 尝试从接收缓冲区取出一条消息
    bool TryPopMessage(std::string& message);
    // 这个函数负责把一条普通字符串打包成：[4字节长度][消息内容]
    bool QueueMessage(const std::string& message);

    bool HasDataToWrite() const;

    void Close();

private:
    static constexpr size_t kHeaderSize = sizeof(uint32_t);
    static constexpr uint32_t kMaxMessageSize = 1024 * 1024;

    Socket socket_;
    // 保存所有已经收到但还没处理完的数据。
    std::string recv_buffer_;
    // 
    std::string send_buffer_;
};

#endif  // INCLUDE_TCP_CONNECTION_H_