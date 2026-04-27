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

    bool Add(int fd, uint32_t events);
    bool Modify(int fd, uint32_t events);
    bool Remove(int fd);

    std::vector<EpollEvent> Wait(int timeout_ms);

    int Fd() const;
private:
    int epoll_fd_;
};



#endif // INCLUDE_EPOLL_H_