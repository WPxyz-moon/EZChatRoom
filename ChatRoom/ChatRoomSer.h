#pragma once
#include <sys/epoll.h>
#include <unordered_map>
#include <unistd.h>
#include "Connection.h"
#include "InetAddr.h"
using namespace std;

//管地址设置，管epoll注册，提供回调接口
//单线程广播用TCP服务器，不用高强度解耦。
//服务器只负责广播，需要注册几个事件
//首先是创建listenfd，用于监听新连接
//然后是对connectfd进行收发；
//如果监听connectfd读，有事件触发
//检测头3个字符
//需要加一个类Package，用于客户端和服务器收发
//Package内包含，本次的客户端请求是什么，以及服务器的回应是什么。
//客户端需要解析，客户端是发消息，还是告知服务器自己的信息还是上传文件，请求下载文件。
//需要一个对象用于序列化和反序列化，组装/解析前缀长度解决粘包。
//这个对象应该是在epoll触发读事件的时候，拿const char*解析，获取对象.
//需要有一个RAII类管控epoll的资源，同时提供epoll_wait
//服务器需要对应的解析。
//用前缀长度解决粘包问题。
//同时还要负责管理Message
//管理Message类，存客户姓名，IP端口，消息时间。
//负责管理Connection
//负责处理Connection处收到的Package


//==>表示资源归属，-->表示依赖
//当前：cntfd ==> Connection --> BufferHandler
//                          ==> Server 
//epollfd                   ==>Server  --> Decoder
//listenfd                  ==>Server


class ChatRoomSer
{
private:
    InetAddr ser_addr_;
    unordered_map<int,Connection*> clients_;
    int epfd_;
    int listenfd_;
public:
    //使用RAII管理epoll和listenfd_
    ChatRoomSer(string ip,uint16_t port):
        ser_addr_(ip,port)
    {   
        epfd_ = epoll_create1(0);
        if(epfd_ == -1){
            cout << "epoll create error";
            exit(-1);
        }
        listenfd_ = socket(AF_INET,SOCK_STREAM,0);
        if(listenfd_ == -1){
            cout << "listenfd create error";
            exit(-1);
        }
    }

    ~ChatRoomSer(){
        close(epfd_);
        close(listenfd_);
    }
    string address() const { return ser_addr_.address();}
    int connectionNums() const { return clients_.size();} 
    void loop();
};
