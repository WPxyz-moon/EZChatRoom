#pragma once
#include "EventHandler.h"

#include <string>

class ChatRoom;

// 把标准输入（fd 0）也当成一个 EventHandler 注册进同一个 epoll：
// 单线程下无需另开线程读键盘，命令和网络事件在同一 Loop 里串行处理。
//
// 命令：
//   help / ?          显示帮助
//   list              列出在线用户
//   kick <name>       踢掉指定用户
//   say <text>        以 server 身份广播
//   quit / exit       退出服务器
//
// 生命周期：对象由 ChatRoom 以 unique_ptr 持有并管理；本类析构时
// 不能关闭 fd 0（那是进程的标准输入），所以析构里把 fd_ 置 -1。
class KeyBoardHandler : public EventHandler {
public:
    KeyBoardHandler(EventLoop* loop, ChatRoom* room);
    ~KeyBoardHandler() override;

    bool handleRead() override;

private:
    void processLine(const std::string& line);

    ChatRoom*   room_;
    std::string buffer_;
};
