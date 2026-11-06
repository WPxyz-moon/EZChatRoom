#include "EventLoop.h"
#include "ChatRoom.h"
#include <cstring>

void EventLoop::shutdown(){
    quit_ = true;

    // 1) 只摘除，不回调 onRemoved：此时业务层正在析构，不能再重入业务逻辑
    for (auto& kv : handlers_) {
        EventHandler* h = kv.first;
        int fd = h->getFd();
        if (fd < 0) continue;
        if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) < 0 && errno != ENOENT) {
            std::cout << "[WARN] shutdown: epoll DEL failed, fd=" << fd
                      << " errno=" << std::strerror(errno) << "\n";
        }
    }
    handlers_.clear();

    // 2) 延迟删除队列里的对象所有权已经在 Loop 手里，这里就地回收
    pendingDelHandlers_.clear();
}

void EventLoop::loop(){
    epoll_event evts[MAX_EVENT_NUMS];
    while(!quit_){
        int n = epoll_wait(epfd_,evts,MAX_EVENT_NUMS,-1);   //完全阻塞
        
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cout << "[FATAL] epoll_wait failed, errno=" << errno << "\n";
            break;
        }

        for(int i = 0; i < n; ++i ){
            EventHandler* handler = static_cast<EventHandler*>(evts[i].data.ptr);
            
            if (handlers_.find(handler) == handlers_.end()) continue;

            if((evts[i].events & EPOLLERR) && !handler->handleError()){
                removeHandler(handler);             //去除了就不能再访问了
                continue;
            }

            if((evts[i].events & (EPOLLHUP | EPOLLIN) ) && !handler->handleRead()){
                removeHandler(handler);
                continue;
            }
            
            if((evts[i].events & EPOLLOUT) && !handler->handleWrite()){
                removeHandler(handler);
                continue;
            }
        }

        pendingDelHandlers_.clear();
    }
}

void EventLoop::addHandler(EventHandler* handler,uint32_t events){
    if( !handler || handler->getFd() < 0) return ;
    auto it = handlers_.find(handler);
    if (it != handlers_.end()) {
        cout << "[INFO] addHandler: handler has registered, fd=" 
        << handler->getFd() << ",switch Add to Modify\n";
        modifyHandler(handler,events);
        return;
    }
    epoll_event evt{};
    evt.events = events;
    evt.data.ptr = handler;
    if(epoll_ctl(epfd_,EPOLL_CTL_ADD,handler->getFd(),&evt) < 0){
        std::cout << "[WARN] epoll_ctl ADD failed, fd=" 
                  << handler->getFd() << " errno=" << errno << std::endl;
        return;
    }
    handlers_[handler] = events;
}

void EventLoop::modifyHandler(EventHandler* handler,uint32_t events){
    if( !handler || handler->getFd() < 0) return ;
    auto it = handlers_.find(handler);
    if (it == handlers_.end()) {
        cout << "[WARN] modifyHandler: handler not registered, fd=" << handler->getFd() << "\n";
        return;
    }
    if(it->second == events) return;
    
    epoll_event evt{};
    evt.events = events;
    evt.data.ptr = handler;
    if(epoll_ctl(epfd_,EPOLL_CTL_MOD,handler->getFd(),&evt) < 0){
        std::cout << "[WARN] epoll_ctl MOD failed, fd=" 
                  << handler->getFd() << " errno=" << errno << std::endl;
        return;
    }
    it->second = events;
}

void EventLoop::removeHandler(EventHandler* handler){
    if (!handler) return;
    auto it = handlers_.find(handler);
    if (it == handlers_.end()) return;          // 已不在表里，重复调用安全

    int fd = handler->getFd();
    if (fd >= 0 && epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        int err = errno;
        std::cout << "[WARN] epoll DEL failed, fd=" << fd
                  << " errno=" << std::strerror(err) << "\n";
    }

    handlers_.erase(it);      // 无论 DEL 成败都要摘
    handler->onRemoved();     // 无论 DEL 成败都要通知 owner
}
