#include "epoll.h"

#include <cerrno> // errno
#include <cstring> // sterror
#include <iostream> // std::cerr
#include <sys/epoll.h> // epoll_create1, epoll_ctl, epoll_wait
#include <unistd.h> // close

Epoll::Epoll() : epoll_fd_(-1) {}

Epoll::~Epoll() {
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool Epoll::Create() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        std::cerr << "epoll_create1 failed : "
                  << strerror(errno) << '\n';
        return false;
    }
    return true;
}

bool Epoll::Add(int fd, uint32_t events) {
    epoll_event event{};
    event.data.fd = fd;
    event.events = events;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1) {
        std::cerr << "epoll_ctl ADD failed : "
                  << strerror(errno) << '\n';
        return false;
    }
    return true;
}

bool Epoll::Modify(int fd, uint32_t events) {
    epoll_event event{};
    event.data.fd = fd;
    event.events = events;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == -1) {
        std::cerr << "epoll_ctl MOD failed : "
                  << strerror(errno) << '\n';
        return false;
    }
    return true;
}

bool Epoll::Remove(int fd) {
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        std::cerr << "epoll_ctl DEL failed : "
                  << strerror(errno) << '\n';
        return false;
    }
    return true;
}

// 
std::vector<EpollEvent> Epoll::Wait(int timeout_ms) {
    constexpr int kMaxEvents = 64;
    std::vector<epoll_event> raw_events(kMaxEvents);
    // timeout_ms
    // -1: 一直阻塞，直到有事件发生
    // 0: 立即返回，不等待
    // >0: 最多等待这么多毫秒
    int ready_count = epoll_wait(epoll_fd_,
                                 raw_events.data(),
                                 static_cast<int>(raw_events.size()),
                                 timeout_ms);
    std::vector<EpollEvent> res;
    if (ready_count == -1) {
        // EINTR表示信号被打断
        if (errno != EINTR) {
            std::cerr << "epoll_wait failed: "
                     << strerror(errno) << '\n';
        }
        return {};
    }

    res.reserve(static_cast<size_t>(ready_count));

    for (int i = 0; i < ready_count; i++) {
        res.push_back(EpollEvent{raw_events[i].data.fd, raw_events[i].events});
    }
    return res;
}

int Epoll::Fd() const {
    return epoll_fd_;
}