#ifndef INCLUDE_EPOLL_H_
#define INCLUDE_EPOLL_H_

#include <cstdint> // uint32_t
#include <vector>

struct EpollEvent {
    int fd;
    uint32_t events;
};

class Epoll {
public:
    Epoll();

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    ~Epoll();

    bool Create();

    /**
     * @brief epoll_ctl EPOLL_CTL_ADD：注册新的 fd 到 epfd。
     * 
     * @param fd epoll_fd
     * @param events 事件 EPOLLIN
     * @return true 添加成功
     * @return false 添加失败
     */
    bool Add(int fd, uint32_t events);
    
    /**
     * @brief epoll_ctl EPOLL_CTL_MOD：修改已注册 fd 的监听事件。
     * 
     * @param fd 
     * @param events 
     * @return true 
     * @return false 
     */
    bool Modify(int fd, uint32_t events);
    /**
     * @brief EPOLL_CTL_DEL：从 epfd 中删除一个 fd。
     * 
     * @param fd 
     * @return true 
     * @return false 
     */
    bool Remove(int fd);

    /**
     * @brief 等待事件，并把事件添加到events中
     * 
     * @param timeout_ms 
     * @return std::vector<EpollEvent> 
     */
    std::vector<EpollEvent> Wait(int timeout_ms);

    int Fd() const;
private:
    int epoll_fd_;
};



#endif // INCLUDE_EPOLL_H_