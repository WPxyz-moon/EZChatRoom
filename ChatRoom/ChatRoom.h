#pragma once
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include "Connection.h"
#include "InetAddr.h"
#include "../Reactor/Acceptor.h"
#include "Messages.h"
using namespace std;

class FileManager;

//维护客户端名称
class ChatRoom
{
private:
    InetAddr ser_addr_;
    unordered_map<string,Connection*> onlineClients_;
    unordered_set<Connection*> clients_;
    Acceptor* listener_;
    EventLoop* loop_;
    FileManager* fileManager_;
    //消息处理
    //接口1.按Package的type解析处理Package的body（直接处理干净了，不返回）
    void handleLogin(Connection* sender, const string& body);
    
    //接口2.解析消息，target为空代表为广播，否则就是发给特定目标
    void handleMessage(Connection* sender, const string& body);
public:
    ChatRoom(string ip,uint16_t port):
        ser_addr_(ip,port)
    {   
        int listenfd = socket(AF_INET,SOCK_STREAM,0);
        if(listenfd == -1){
            cout << "listenfd create error";
            exit(-1);
        }
        listener_ = new Acceptor(listenfd,loop_,ip,port);
    }

    ~ChatRoom(){
        
    }

    string address() const { return ser_addr_.strIpPort();}
    int connectionNums() const { return clients_.size();} 
    //注销Connection，不销毁，销毁由Connection自己决定。
    void removeConnection(Connection* target);
    void analysePackages(Connection* sender,const vector<Package>& packages);

};
