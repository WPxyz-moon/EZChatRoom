#include "KeyBoardHandler.h"

#include "ChatRoom.h"
#include "EventLoop.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace {

std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    size_t e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

}  // namespace

KeyBoardHandler::KeyBoardHandler(EventLoop* loop, ChatRoom* room)
    : EventHandler(0, loop), room_(room) {
    // stdin 默认是阻塞的，不设非阻塞会在 read 循环里把整个 Loop 卡住
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    }
    loop_->addHandler(this, EPOLLIN);
}

KeyBoardHandler::~KeyBoardHandler() {
    // 标准输入不属于本对象，禁止基类 close(0)
    fd_ = -1;
}

bool KeyBoardHandler::handleRead() {
    char buf[1024];
    bool eof = false;

    for (;;) {
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            buffer_.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) {           // stdin 被关闭/管道 EOF
            eof = true;
            break;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        eof = true;
        break;
    }

    // 先处理所有完整行，保证管道一次性灌入的命令（包括 quit）都被执行
    size_t pos;
    while ((pos = buffer_.find('\n')) != std::string::npos) {
        std::string line = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 1);
        processLine(line);
    }

    if (eof) {
        if (!buffer_.empty()) {
            processLine(buffer_);
            buffer_.clear();
        }
        return false;   // 从 epoll 注销；对象仍由 ChatRoom 持有
    }

    if (buffer_.size() > 4096) buffer_.clear();   // 防御无换行的超长输入
    return true;
}

void KeyBoardHandler::processLine(const std::string& raw) {
    std::string line = trim(raw);
    if (line.empty()) return;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "help" || cmd == "?") {
        std::cout << "commands:\n"
                  << "  list          show online users\n"
                  << "  kick <name>   disconnect a user\n"
                  << "  say <text>    broadcast as server\n"
                  << "  quit / exit   stop server\n";
    } else if (cmd == "list") {
        std::vector<std::string> names = room_->onlineNames();
        std::cout << "online (" << names.size() << "):";
        if (names.empty()) {
            std::cout << " -";
        } else {
            for (const auto& n : names) std::cout << " " << n;
        }
        std::cout << std::endl;
    } else if (cmd == "kick") {
        std::string name;
        iss >> name;
        if (name.empty()) {
            std::cout << "usage: kick <name>" << std::endl;
        } else if (room_->kick(name)) {
            std::cout << "[INFO] kicked " << name << std::endl;
        } else {
            std::cout << "[WARN] no such online user: " << name << std::endl;
        }
    } else if (cmd == "say") {
        std::string text;
        std::getline(iss, text);
        text = trim(text);
        if (text.empty()) {
            std::cout << "usage: say <text>" << std::endl;
        } else {
            room_->broadcastFromServer(text);
            std::cout << "[server] " << text << std::endl;
        }
    } else if (cmd == "quit" || cmd == "exit") {
        std::cout << "[INFO] shutting down..." << std::endl;
        room_->quit();
    } else {
        std::cout << "[WARN] unknown command: " << cmd
                  << " (type help)" << std::endl;
    }
}
