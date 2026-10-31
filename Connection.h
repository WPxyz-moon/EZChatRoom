#pragma once
#include "InetAddr.h"
#include "Package.h"
#include <string>
#include <unistd.h>
#include <vector>
//负责管理连接fd的释放，即RAII
//同时负责解决TCP粘包问题.
class Connection
{
private:
    int fd_;
    string readBuffer_;
    string writeBuffer_;
    InetAddr addr_;
    string cliname_;
public:
    Connection(int fd,const InetAddr& addr) :fd_(fd),addr_(addr){}
    ~Connection(){
        if(fd_ >= 0){
            close(fd_);
            fd_ = -1;
        }
    }
    vector<Package> recv();
    void send(const Package& pack);
    // 禁止拷贝和移动
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;
};

