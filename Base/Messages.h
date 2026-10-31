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
// 客户端发 MESSAGE：cliName_ 填、time_ 留 0
// 服务端广播 MESSAGE_BROADCAST：cliName_ 原样、time_ 填服务端时间戳
struct Message {
    uint64_t    time_ = 0;     // 服务端时间戳（秒），0=未填；线格式用 uint64_t 定宽
    std::string cliName_;
    std::string text;
};

// ERROR_MSG: 通用错误（双向）
struct ErrorMsg {
    uint16_t    code;          // 错误码（必须填，0 语义矛盾，不给默认值）
    std::string reason;
};