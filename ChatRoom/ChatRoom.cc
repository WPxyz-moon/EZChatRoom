#include "ChatRoom.h"
#include "codec.h"
#include <chrono>

using namespace codec;

static inline uint64_t getCurrentTimeSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

static inline void replyError(Connection* sender, int code, const std::string& reason) {
    ErrorMsg err;
    err.code = code;
    err.reason = reason;
    sender->send(Package(PackType::ERROR_MSG, encodeErrorMsg(err)));
}

void ChatRoom::analysePackages(Connection* sender,const vector<Package>& packages){
    for(const auto& pack : packages){
        switch(pack.type_){
            case PackType::LOGIN:
                handleLogin(sender,pack.body_);
                break;
            case PackType::MESSAGE:
                handleMessage(sender,pack.body_);
                break;
            default:
                break;
        }
    }
}

void ChatRoom::handleLogin(Connection* sender, const string& body){
    LoginReq req;
    LoginAck ack;
    ack.code = 0;
    if(decodeLogin(body,req)){
        auto [it, inserted] = onlineClients_.insert({req.name, sender});
        
        if(inserted){                   //新增成功
            cout << "[INFO] "<< req.name << " Login Success" << endl;
            sender->setName(req.name);
        }else if(it->second != sender){ //已存在其他连接(插入失败)
            ack.code = 1;
            ack.reason = "Client \"" + req.name + "\" has existed";
        }else{                          //重复登录
            cout << "[INFO] "<< req.name << " Login repeatedly" << endl;
            ack.reason = "Already logged in";
        }

    }else{
        ack.code = -1;
        ack.reason = "Login decode error!";
    }
    
    sender->send(Package(PackType::LOGIN_ACK,encodeLoginAck(ack)));
}

void ChatRoom::handleMessage(Connection* sender, const string& body){
    Message msg;
    if(!decodeMessage(body, msg)){
        replyError(sender, 0, "Message decode error!");
        return;
    }

    //发送者必须已经登录
    std::string senderName = sender->getName();
    if(senderName.empty()){
        replyError(sender, 2, "Please login first.");
        return;
    }
    
    //文本非空
    if(msg.text.empty()){
        replyError(sender, 1, "Message text cannot be empty!");
        return;
    }

    Message retMsg{getCurrentTimeSeconds(), senderName, msg.text};
    const string retbody = encodeMessage(retMsg);

    //广播
    if(msg.cliName.empty()){ 
        for(auto& [name, conn] : onlineClients_){
            conn->send(Package(PackType::MESSAGE,retbody));
        }
    } else { //私聊 
        auto it = onlineClients_.find(msg.cliName);
        if(it == onlineClients_.end()){
            replyError(sender, 1, "User \"" + msg.cliName + "\" not found or offline.");
            return;
        }
        if(it->second != sender){
            it->second->send(Package(PackType::MESSAGE, retbody));
        }
        sender->send(Package(PackType::MESSAGE, retbody));
    }
}

void ChatRoom::removeConnection(Connection* target) {
    if (!target) return;

    //从在线名单中去掉
    const std::string& name = target->getName();
    if (!name.empty()) {
        auto it = onlineClients_.find(name);
        if (it != onlineClients_.end() && it->second == target) {
            onlineClients_.erase(it);
        }
    }

    //取消连接监视
    clients_.erase(target);
    
}

//room死时，让loop标记删除connection，
ChatRoom::ChatRoom(string ip,uint16_t port):
    ser_addr_(ip,port)
{   

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd == -1){
        cout << "listenfd create error";
        exit(-1);
    }
    listener_ = new Acceptor(listenfd,loop_,ip,port);
}

ChatRoom::~ChatRoom(){
    
}