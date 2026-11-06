#pragma once
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <unistd.h>
#include "Connection.h"
#include "InetAddr.h"
#include "../Reactor/Acceptor.h"
#include "Messages.h"
#include "EventLoop.h"
#include "FileManager.h"
using namespace std;


//维护客户端名称,什么周期也由Loop管理
//
class ChatRoom
{
private:
    using ConnectionPtr = unique_ptr<Connection>; 

    InetAddr ser_addr_;
    unordered_map<string,Connection*> onlineClients_;
    unordered_map<Connection *,ConnectionPtr> clients_;
    unique_ptr<Acceptor> listener_;
    EventLoop loop_;
    FileManager fileManager_;
    //消息处理
    //接口1.按Package的type解析处理Package的body（直接处理干净了，不返回）
    void handleLogin(Connection* sender, const string& body);
    
    //接口2.解析消息，target为空代表为广播，否则就是发给特定目标
    void handleMessage(Connection* sender, const string& body);
public:
    ChatRoom(string ip,uint16_t port);
    ~ChatRoom();

    string address() const { return ser_addr_.strIpPort();}
    int connectionNums() const { return clients_.size();} 
    //注销Connection，不销毁，销毁由Connection自己决定。
    void removeConnection(Connection* target);
    void analysePackages(Connection* sender,const vector<Package>& packages);

};
