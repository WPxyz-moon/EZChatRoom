#pragma once
#include "Package.h"
#include <vector>

class EventHandler
{
public:
    virtual int getFd() const {return fd_;}
    virtual bool handleRead() {return true;}
    virtual bool handleRead(vector<Package>& packges) {return true;};              //要监听的fd基本都有读事件
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
    EventHandler(int fd) : fd_(fd) {}
    int fd_;
};
