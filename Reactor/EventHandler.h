#pragma once
#include "Package.h"
#include <vector>

class EventLoop;
class EventHandler
{
public:
    virtual int getFd() const {return fd_;}
    virtual bool handleRead() = 0;
    virtual bool handleWrite() {return true;}   //写只有Connection写才有
    virtual bool handleError() {return false;}  //默认遇到错误返回false，让Epoll准备注销fd
    virtual void onRemoved() {}
    virtual ~EventHandler() {
        if(fd_ >= 0){
            close(fd_);
            fd_ = -1;
        }
    }
protected:
    EventHandler(int fd,EventLoop* loop) : fd_(fd),loop_(loop){}
    int fd_;
    EventLoop* loop_;       //用loop更新监听的事件
};
