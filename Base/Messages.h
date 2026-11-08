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

// MESSAGE / MESSAGE_BROADCAST: 聊天消息
// 客户端发 MESSAGE： cliName空代表广播，否则代表定向发送,time_ 留 0
// 服务端转发 MESSAGE：time_ 填服务端时间戳
struct Message {
    uint64_t    time = 0;     // 服务端时间戳（秒），0=未填；线格式用 uint64_t 定宽
    std::string cliName;      // C->S表示目标客户端名称，S->C表示来源客户端名称
    std::string text;
    // S->C：私聊接收者名称，空=广播；C->S不使用。
    // 线格式放在最后且可选，旧版本对端可以忽略它，保持兼容。
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