#pragma once
#include <sys/epoll.h>
#include <unordered_map>
#include <unistd.h>
#include "Connection.h"
#include "InetAddr.h"
#include "../Reactor/Acceptor.h"
#include "Messages.h"
using namespace std;

#include <list>

class FileManager;
class MessageManager;


class ChatRoom
{
private:
    InetAddr ser_addr_;
    unordered_set<Connection*> clients_;
    Acceptor* listener_;
    EventLoop* loop_;
    FileManager* fileManager_;
    MessageManager* messageManager_;
    list<Package*> readyPackages_;
public:
    ChatRoom(string ip,uint16_t port):
        ser_addr_(ip,port)
    {   

        int listenfd = socket(AF_INET,SOCK_STREAM,0);
        if(listenfd == -1){
            cout << "listenfd create error";
            exit(-1);
        }
        listener_ = new Acceptor(listenfd,loop_,"129.168.253.130",6789);
    }

    ~ChatRoom(){
        
    }

    string address() const { return ser_addr_.strIpPort();}
    int connectionNums() const { return clients_.size();} 
    void removeConnection(Connection* target);
    void analysePackages(const vector<Package>& packages);
};
