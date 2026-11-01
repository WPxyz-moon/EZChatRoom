#pragma once
#include "InetAddr.h"
#include "Package.h"
#include "EventHandler.h"
#include <string>
#include <unistd.h>
#include <vector>

//负责管理连接fd的释放，即RAII
//同时负责解决TCP粘包问题.
class ChatRoom;
class Connection : public EventHandler
{
private:
    string readBuffer_;
    string writeBuffer_;
    InetAddr addr_;
    string cliname_;
    ChatRoom* room_;                //用于事件触发时 在ChatRomm设置/取消监视。
public:
    Connection(int fd,const InetAddr& addr) :EventHandler(fd),addr_(addr){
        cout << addr_.address() << "建立连接成功" << endl;
    }
    ~Connection(){
        if(fd_ >= 0){
            close(fd_);
            fd_ = -1;
            cout << addr_.address() << "断开连接" << endl;
        }
    }
    
    // 禁止拷贝和移动
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    //返回false表示连接失效，功能是EPOLLIN触发的时候转为packages
    bool handleRead(vector<Package>& packages) override;
    int getFd() const override { return fd_;}
    bool send(const Package& pack);
    
};

