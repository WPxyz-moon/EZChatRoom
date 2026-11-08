#pragma once
#include <cstdint>
#include <string>

// LOGIN: C→S 用户名
struct LoginReq {
    std::string name;
};

// LOGIN_ACK: S→C 登录结果
struct LoginAck {
    uint16_t    code = 0;      // 0=成功，非0=失败（重名/非法名等）
    std::string reason;        // 失败原因，成功时为空
};

// MESSAGE: 聊天消息（双向）
// sender：发送者名称。C->S 由客户端填写；S->C 由服务器填写（服务器通知为 "server"）。
//         服务器路由时以连接上已登录的用户名为准，不信任 C->S 的 sender。
// target：接收者名称。空 = 广播，非空 = 私聊；双向含义一致。
// time  ：C->S 留 0，S->C 为服务端时间戳（秒）。
struct Message {
    uint64_t    time = 0;
    std::string sender;
    std::string text;
    std::string target;
};

// ERROR_MSG: 通用错误（双向）
// code : 0 代表    解码错误
//        1 代表    请求非法
//        2 代表    服务器状态错误
struct ErrorMsg {
    uint16_t    code;          // 错误码（必须填）
    std::string reason;
};