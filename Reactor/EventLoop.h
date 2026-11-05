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
    unordered_set<EventHandler*> pendingDelHandlers_;
    static constexpr int MAX_EVENT_NUMS = 1024; 

    void deletePendings(){
        for(auto handler : pendingDelHandlers_){
            if(handler) delete handler;
        }
        pendingDelHandlers_.clear();
    }
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

    bool isPendingToDelete(Connection* conn){
        return pendingDelHandlers_.find(conn) != pendingDelHandlers_.end();
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

        deletePendings();                        // 把 pending 里的也 delete 掉
        if(epfd_ >= 0){ close(epfd_); epfd_ = -1; }
    }
};
