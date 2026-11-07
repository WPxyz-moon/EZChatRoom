#include "Acceptor.h"
#include "EventLoop.h"
#include "Connection.h"
#include "ChatRoom.h"

Acceptor::~Acceptor() = default;

Acceptor::Acceptor(int fd,EventLoop* loop,std::string ip,uint16_t port,ChatRoom* room):
    EventHandler(fd,loop),
    addr_(ip,port),
    room_(room)
{
    //运行快速重新绑定
    int opt = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(::bind(fd_,addr_.data(),addr_.len()) < 0){
        cout << "[WARN] acceptor bind error ipPort : " << addr_.strIpPort() << endl;
        return;
    }

    if(::listen(fd_,SOMAXCONN) < 0){
        std::cout << "[WARN] listen failed, fd=" << fd_ << " errno=" << errno << "\n";
        return;
    }

    loop_->addHandler(this,EPOLLIN);
}

bool Acceptor::handleRead() {
    while(true){
        sockaddr_in sin{};
        socklen_t slen = sizeof(sin);
        int connfd = ::accept4(fd_,
                        reinterpret_cast<sockaddr*>(&sin),  // 转 sockaddr* 是必须的
                        &slen,
                        SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(connfd < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;   // 取完了，正常
            if (errno == EINTR) continue;                          // 被打断，重试
            std::cout << "[WARN] connect failed, fd=" << fd_ << " errno=" << errno << "\n";
            return true;
        }
        auto conn = make_unique<Connection>(connfd,InetAddr(sin),loop_,room_);
        loop_->addHandler(conn.get(),EPOLLIN);
        room_->addConnection(std::move(conn));
    }
    return true;
}
