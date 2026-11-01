#pragma once
#include "EventHandler.h"

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
//负责管理epoll生命周期，并提供注册管理，同时负责循环解析epoll返回的结果，调用Handler处理
//负责在每轮loop结束，统一销毁Handler，构造由Handler/外部负责。
//EventLoop是生命周期的实质管理者，负责对象的delete，
//但是不代表 资源所有者只是 EventLoop， 完全可以是其他对象，可以触发时机通知Loop摧毁Handler
class EventLoop
{
private:
    int epfd_;
    unordered_map<EventHandler*,uint32_t> handlers_;
    unordered_set<EventHandler*> pendingDelHanlers_;
    static constexpr int MAX_EVENT_NUMS = 1024; 

    void deltePendings(){
        for(auto handler : pendingDelHanlers_){
            if(handler) delete handler;
        }
        pendingDelHanlers_.clear();
    }
public:
    void loop();
    void addHandler(EventHandler* handler,uint32_t events);
    void modifyHandler(EventHandler* handler,uint32_t events);
    void removeHandler(EventHandler* handler);
    void pendToDel(EventHandler* handler){
        pendingDelHanlers_.insert(handler);
    }
    EventLoop(){
        epfd_ = epoll_create1(0);
        if (epfd_ == -1) {
            std::cout << "[WARN] epoll create fail" << std::endl;
        }
    }
    ~EventLoop(){
        for(auto handler : handlers_){
            removeHandler(handler.first);
        }
        handlers_.clear();
        if(epfd_ >= 0){
            close(epfd_);
            epfd_ = -1;
        }
    }
};
