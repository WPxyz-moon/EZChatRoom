#pragma once
#include <netinet/in.h>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <assert.h>

using namespace std;

class InetAddr
{
private:
    static constexpr int IPV4_MAXSIZE = 16;
    sockaddr_in sin_;
public:
    string strIpPort() const{ 
        return ip() + ":" + to_string(port());
    }
    string ip() const {
        char buf[IPV4_MAXSIZE];
        memset(buf,0,sizeof(buf));
        if(inet_ntop(AF_INET,&sin_.sin_addr,buf,sizeof(buf)) == nullptr){
            cout << "[ERROR] Invalid IP while getIP";
        }
        return buf;
    }
    uint16_t port() const{
        return ntohs(sin_.sin_port); 
    }

    sockaddr* data(){ return reinterpret_cast<sockaddr*>(&sin_);}
    socklen_t len(){ return sizeof(sin_);}

    //需要自行确保传入的是有效IPv4值
    InetAddr(std::string ip,uint16_t port){
        memset(&sin_,0,sizeof(sin_));
        sin_.sin_family = AF_INET;
        sin_.sin_port =  htons(port); 
        if(inet_pton(AF_INET,ip.c_str(),&sin_.sin_addr) != 1){
            cout << "[ERROR] Invalid IP while InetAddr init";
        }
    }
    explicit InetAddr(const sockaddr_in& sin): sin_(sin){
        assert(sin.sin_family == AF_INET);
    }
    ~InetAddr(){}
};
