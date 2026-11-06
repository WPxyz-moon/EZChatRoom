#pragma once
#include "EventHandler.h"

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
//负责执行外部的handler回调，保证执行期间执行者的生命周期
//于此同时绝对不允许在回调内部杀死ChatRoom导致Loop死亡/Connection非自然死亡

class EventLoop
{
private:
    int epfd_;
    using EventHandlerptr = std::unique_ptr<EventHandler>;
    //handler所有权在ChatRoom，room不死执行期间不死，不提供执行期间杀死room的接口
    //需要成员死去时，且room转移所有权给Loop延迟删除,避免函数内部删除自己。
    unordered_map<EventHandler*,uint32_t> handlers_;       
    unordered_set<EventHandlerptr> pendingDelHandlers_;
    
    static constexpr int MAX_EVENT_NUMS = 1024; 

public:
    void loop();

    void addHandler(EventHandler* handler,uint32_t events);

    void modifyHandler(EventHandler* handler,uint32_t events);

    //取消监听的同时标记删除Handler对象。
    void removeHandler(EventHandler* handler);

    void enableRead(EventHandler* handler){
        uint32_t events = getEvents(handler);
        if(events > 0){
            events |= EPOLLIN;
            modifyHandler(handler,events);
        }
    }

    void disableRead(EventHandler* handler){
        uint32_t events = getEvents(handler);
        if(events > 0){
            events &= ~EPOLLIN;
            modifyHandler(handler,events);
        }
    }

    void enableWrite(EventHandler* handler){
        uint32_t events = getEvents(handler);
        if(events > 0){
            events |= EPOLLOUT;
            modifyHandler(handler,events);
        }
    }

    void disableWrite(EventHandler* handler){
        uint32_t events = getEvents(handler);
        if(events > 0){
            events &= ~EPOLLOUT;
            modifyHandler(handler,events);
        }
    }

    //0表示没有找到/没有监听任何事件
    uint32_t getEvents(EventHandler* handler) const {
        if(handler == nullptr) return 0;
        auto it = handlers_.find(handler);
        return it == handlers_.end() ? 0 : it->second;
    }

    void pendToKillHandler(EventHandlerptr ptr){
        pendingDelHandlers_.insert(ptr);
    }

    EventLoop(){
        epfd_ = epoll_create1(0);
        if (epfd_ == -1) {
            std::cout << "[WARN] epoll create fail" << std::endl;
        }
    }
    ~EventLoop(){
        std::vector<EventHandler*> all;
        all.reserve(handlers_.size());
        for (auto& kv : handlers_) all.push_back(kv.first);

        for (auto* h : all) removeHandler(h);
        handlers_.clear();

        pendingDelHandlers_.clear();
        if(epfd_ >= 0){ close(epfd_); epfd_ = -1; }
    }
};
