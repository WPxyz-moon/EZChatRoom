#include "../Reactor/Connection.h"
#include "../Base/codec_pack.h"
#include "EventLoop.h"
#include "ChatRoom.h"

#include <sys/types.h>   // ssize_t 等类型的定义
#include <sys/socket.h>  // socket 相关的系统调用
#include <unistd.h>      // read() 函数
#include <cerrno>        // errno 错误码（EAGAIN, EWOULDBLOCK, EINTR）

//RAII,false代表连接失效了，需要上层解除监听并清除对象以断开
bool Connection::handleRead(){
    vector<Package> packages;
    if (fd_ < 0) return false; 
    ssize_t n = 1;
    char buf[65536];
    while(n > 0){
        n = read(fd_,buf,sizeof(buf));
        if(n > 0){
            readBuffer_.append(buf,n);
        } else if(n == 0){
            return false;
        } else{
            // 3. 处理错误
            if (errno == EAGAIN || errno == EWOULDBLOCK) {// 核心：数据读干净了，正常退出循环
                break;
            } else if (errno == EINTR) {// 被系统信号打断，继续重试
                continue;
            } else {                    // 其他致命错误（如 ECONNRESET对端强制掐断）
                return false;
            }
        }
    }
    if (codec::parse(readBuffer_,packages) == false) 
    {
        cout << "[WARN] " << addr_.strIpPort() << "Package Wrong" << endl;
        return false;
    }
    
    room_->analysePackages(this,packages);
    return true;
}

bool Connection::send(const Package& pack) {
    if (fd_ < 0) return false; 
    writeBuffer_.append(codec::serialize(pack));
    return handleWrite();
}

// 把写缓冲区的东西发送出去(能直接发尽量直接发而不是监听)
bool Connection::handleWrite() {
    ssize_t n = ::send(fd_, writeBuffer_.data(), writeBuffer_.size(), MSG_NOSIGNAL);
    //MSG_NOSIGNAL防对端残留信息导致进程奔溃
    if (n < 0) {
        // 缓冲满
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            loop_->enableWrite(this);   
            return true;
        }
        std::cout << "[WARN] send failed, fd=" << fd_
                  << " errno=" << errno << "\n";
        return false;
    }

    if (n > 0) {
        writeBuffer_.erase(0, static_cast<size_t>(n)); 
    }

    if (writeBuffer_.empty()) {
        loop_->disableWrite(this);
    } else {
        loop_->enableWrite(this);
    }
    return true;
}

//通知room_注销连接,交给Loop销毁
void Connection::onRemoved() {
    room_->removeConnection(this);
}
