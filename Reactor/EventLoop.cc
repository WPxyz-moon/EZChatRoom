#include "EventLoop.h"
#include <cstring>

void EventLoop::loop(){
    epoll_event evts[MAX_EVENT_NUMS];
    while(true){
        int n = epoll_wait(epfd_,evts,MAX_EVENT_NUMS,-1);   //完全阻塞
        
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cout << "[FATAL] epoll_wait failed, errno=" << errno << "\n";
            break;
        }

        for(int i = 0; i < n; ++i ){
            EventHandler* handler = static_cast<EventHandler*>(evts[i].data.ptr);
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

        deletePendings();
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
    if( !handler || handler->getFd() < 0) return ;
    auto it = handlers_.find(handler);
    if (it == handlers_.end()) {
        cout << "[WARN] removeHandler: handler not registered, fd=" << handler->getFd() << "\n";
        return;
    }
    
    if(epoll_ctl(epfd_,EPOLL_CTL_DEL,handler->getFd(),nullptr) < 0){
        int err = errno;
        std::cout << "[WARN] epoll_ctl DELETE failed, fd=" 
                  << handler->getFd() << " errno=" << std::strerror(err) << std::endl;
        if (err == ENOENT) {
            handlers_.erase(it);   // epoll 里已不存在，清掉脏记录
        }
        return;
    }
    handlers_.erase(it);
    handler->onRemoved();
    pendingDelHandlers_.insert(handler);
}