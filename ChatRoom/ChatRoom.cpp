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
            case PackType::FILE_UPLOAD_META:
                fileManager_.onUploadMeta(sender, pack.body_);
                break;
            case PackType::FILE_UPLOAD_CHUNK:
                fileManager_.onUploadChunk(sender, pack.body_);
                break;
            case PackType::FILE_UPLOAD_DONE:
                fileManager_.onUploadDone(sender, pack.body_);
                break;
            case PackType::FILE_DOWNLOAD_REQ:
                fileManager_.onDownloadReq(sender, pack.body_);
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

    // target 回填：广播为空，私聊为接收者名称（发送者和接收者都能据此区分）
    Message retMsg{getCurrentTimeSeconds(), senderName, msg.text, msg.cliName};
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

void ChatRoom::addConnection(ConnectionPtr conn){
    Connection* raw_ptr = conn.get();
    clients_.emplace(raw_ptr, std::move(conn));
}

// 只应由 Connection::onRemoved() 调用（见 ChatRoom.h 中的生命周期契约）。
// 业务层主动断开连接请走 loop_.removeHandler()。
void ChatRoom::removeConnection(Connection* target) {
    if (!target) return;

    // 先取消该连接的上传/下载会话：此时 Connection 还活着，
    // 可以安全地清掉它身上的 onWritable 回调并关闭会话文件。
    fileManager_.cancelConnection(target);

    //从在线名单中去掉
    const std::string& name = target->getName();
    if (!name.empty()) {
        auto it = onlineClients_.find(name);
        if (it != onlineClients_.end() && it->second == target) {
            onlineClients_.erase(it);
        }
    }
    
    auto clientIt = clients_.find(target);
    if (clientIt != clients_.end()) {
        loop_.pendToKillHandler(std::move(clientIt->second));
        clients_.erase(clientIt);
    }
    
}

void ChatRoom::start(){
    loop_.loop();
}

//room死时，让loop标记删除connection，
ChatRoom::ChatRoom(string ip,uint16_t port):
    ser_addr_(ip,port)
{   
    // 必须非阻塞：Acceptor::handleRead 会循环 accept4 直到 EAGAIN，
    // 阻塞的监听 fd 会在第二次 accept4 时把整个单线程 Loop 卡死。
    int listenfd = socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK,0);
    if(listenfd == -1){
        cout << "listenfd create error";
        exit(-1);
    }
    listener_ = make_unique<Acceptor>(listenfd,&loop_,ip,port,this);

    // 标准输入也注册进同一个 Loop；失败（如 stdin 不是可 poll 的 fd）只告警
    keyboard_ = make_unique<KeyBoardHandler>(&loop_, this);
}

// 析构顺序由本函数显式保证，不依赖成员声明顺序：
// 1) fileManager_.cancelAll()：此时全部 Connection 仍然存活，
//    可以安全地关闭传输会话、清掉 onWritable 回调；
// 2) loop_.shutdown()：停止循环，把所有 Handler 从 epoll/注册表摘除，
//    并回收延迟删除队列里已经移交所有权给 Loop 的 Connection；
// 3) clients_.clear()：销毁仍由 Room 拥有的 Connection。
// 顺序不能反：先 shutdown 会回收 Connection，FileManager 里的
// Connection* 会话索引就会变成悬垂指针。
ChatRoom::~ChatRoom(){
    fileManager_.cancelAll();
    loop_.shutdown();
    clients_.clear();
}

bool ChatRoom::kick(const string& name) {
    auto it = onlineClients_.find(name);
    if (it == onlineClients_.end()) return false;

    Connection* conn = it->second;
    // 走 Loop 注销，触发 onRemoved → removeConnection，对象由 Loop 延迟删除
    loop_.removeHandler(conn);
    return true;
}

void ChatRoom::broadcastFromServer(const string& text) {
    Message msg{getCurrentTimeSeconds(), "server", text, ""};
    const string body = encodeMessage(msg);
    for (auto& kv : onlineClients_) {
        kv.second->send(Package(PackType::MESSAGE, body));
    }
}

vector<string> ChatRoom::onlineNames() const {
    vector<string> names;
    names.reserve(onlineClients_.size());
    for (const auto& kv : onlineClients_) {
        names.push_back(kv.first);
    }
    return names;
}