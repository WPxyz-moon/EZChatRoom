#pragma once
#include "EventHandler.h"
#include "InetAddr.h"

class ChatRoom;
class Acceptor : public EventHandler
{
private:
    InetAddr addr_;
    ChatRoom* room_;
public:
    //构造时fd必须是有效资源
    Acceptor(int fd,EventLoop* loop,std::string ip,uint16_t port,ChatRoom* room) ;

    bool handleRead() override;
    ~Acceptor();
};

