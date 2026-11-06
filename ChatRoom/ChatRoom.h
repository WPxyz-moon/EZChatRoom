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
#include "../Reactor/KeyBoardHandler.h"
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
    // 成员按声明逆序析构：fileManager_ 必须声明在 loop_ 之前，
    // 保证 Loop 回收 Connection 时 FileManager 仍然存活。
    FileManager fileManager_;
    unique_ptr<Acceptor> listener_;
    // 键盘 Handler 必须在 loop_ 之前声明：Loop 析构时仍持有指向它的注册项，
    // 若它先被销毁，Loop 析构里的 removeHandler 就是 UAF。
    unique_ptr<KeyBoardHandler> keyboard_;
    EventLoop loop_;   // 最后声明 → 最先析构（它的析构会回调 ChatRoom 的其他成员）
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
    void addConnection(ConnectionPtr conn);

    // ⚠ 生命周期契约：本函数只允许由 EventLoop::removeHandler() 触发的
    // Connection::onRemoved() 调用。此时 epoll 已先注销，本函数把对象所有权
    // 交给 Loop 延迟删除，所以不能在回调栈上直接 delete。
    //
    // 业务层若要主动断开连接（踢人/超时/传输失败等），必须调用
    // loop_.removeHandler(conn)，绝不能直接调用本函数：
    // 直接调用会跳过 epoll 注销，epoll 中会残留指向已销毁对象的 data.ptr，
    // 下一轮 epoll_wait 分发即 UAF。
    void removeConnection(Connection* target);
    void analysePackages(Connection* sender,const vector<Package>& packages);
    void start();

    // ===== 控制台（KeyBoardHandler）用的接口 =====
    void quit() { loop_.quit(); }
    bool kick(const string& name);
    void broadcastFromServer(const string& text);
    vector<string> onlineNames() const;
};
