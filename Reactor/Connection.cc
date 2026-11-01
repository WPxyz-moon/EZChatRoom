#include "Connection.h"
#include "../Base/codec_pack.h"
#include <sys/types.h>   // ssize_t 等类型的定义
#include <sys/socket.h>  // socket 相关的系统调用
#include <unistd.h>      // read() 函数
#include <cerrno>        // errno 错误码（EAGAIN, EWOULDBLOCK, EINTR）

//RAII,false代表连接失效了，需要上层解除监听并清除对象以断开
bool Connection::handleRead(vector<Package>& packages){
    if (fd_ < 0) return false; 
    ssize_t n = 1;
    char buf[65536];
    while(n > 0){
        n = read(fd_,buf,sizeof(buf));
        if(n > 0){
            readBuffer_.append(buf,n);
            if(n < static_cast<ssize_t>(sizeof(buf))) break;
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
        cout << "[WARN] " << addr_.address() << "Package Wrong" << endl;
        return false;
    }
    return true;
}

//返回false代表发送失败，需要epoll监听EPOLLOUT
bool Connection::send(const Package& pack) {
    if (fd_ < 0) return false; 
    ssize_t n;
    writeBuffer_.append(codec::serialize(pack));
    n = write(fd_,writeBuffer_.data(),writeBuffer_.size());
    
    
    return true;
}