#pragma once
#include "InetAddr.h"
#include "Package.h"
#include "EventHandler.h"
#include <functional>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

//负责管理连接fd的释放，即RAII
//同时负责解决TCP粘包问题.
class EventLoop; 
class ChatRoom; 
class Connection : public EventHandler
{
private:
    string readBuffer_;
    string writeBuffer_;
    InetAddr addr_;
    string cliname_;
    ChatRoom* room_;            //用于通知room删除监视信息/通知room解析Package
    // 写缓冲排空后调用（下载泵用它继续灌数据）。回调内允许清空自身。
    std::function<void()> onWritable_;
    bool inWriteCallback_ = false;
public:
    Connection(int fd,const InetAddr& addr,EventLoop* loop,ChatRoom* room);
    ~Connection(){}
    
    // 禁止拷贝和移动
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    //返回false表示连接失效，功能是EPOLLIN触发的时候转为packages
    bool handleRead() override;
    bool handleWrite() override;
    void onRemoved() override;
    bool send(const Package& pack);
    string getName() const { return cliname_;}
    void setName(const string& name) { cliname_ = name; }

    // 供文件下载做背压控制：已排队但还没发出去的字节数
    size_t bufferedBytes() const { return writeBuffer_.size(); }
    void setOnWritable(std::function<void()> cb) { onWritable_ = std::move(cb); }
};

