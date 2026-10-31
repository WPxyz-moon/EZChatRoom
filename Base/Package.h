#pragma once
#include <cstdint>
#include <string>

using namespace std;
constexpr size_t PACK_HEADER_SIZE = sizeof(PackHeader);
constexpr uint32_t MAGICNUM = 0xaebecbdd;
constexpr uint32_t MAX_BODYLEN = 1024*1024;

enum class PackType : uint16_t {
    // ===== 0x01 段：连接/登录 =====
    LOGIN              = 0x0101,   // C→S 用户名
    LOGIN_ACK          = 0x0102,   // S→C 登录结果

    // ===== 0x02 段：聊天消息 =====
    MESSAGE            = 0x0201,   // C→S 发消息
    MESSAGE_BROADCAST  = 0x0202,   // S→C 广播（可和 MESSAGE 共用 body）

    // ===== 0x03 段：文件上传 =====
    FILE_UPLOAD_META        = 0x0301,  // C→S 文件元信息
    FILE_UPLOAD_META_ACK    = 0x0302,  // S→C 同意/拒绝
    FILE_UPLOAD_CHUNK       = 0x0303,  // C→S 文件块
    FILE_UPLOAD_DONE        = 0x0304,  // C→S 上传完成
    FILE_UPLOAD_DONE_ACK    = 0x0305,  // S→C 完成确认（含校验结果）

    // ===== 0x04 段：文件下载 =====
    FILE_DOWNLOAD_REQ       = 0x0401,  // C→S 请求下载
    FILE_DOWNLOAD_META      = 0x0402,  // S→C 文件元信息
    FILE_DOWNLOAD_CHUNK     = 0x0403,  // S→C 文件块
    FILE_DOWNLOAD_DONE      = 0x0404,  // S→C 下载完成

    // ===== 0xF0 段：系统 =====
    ERROR_MSG               = 0xF001,  // 通用错误
};

#pragma pack(push,1)         //改变对齐粒度为1
struct PackHeader
{
    uint32_t magic_;         //用于标识包开头
    uint32_t bodyLen_;
    uint32_t seq_;           //用于标识包的顺序（客户端递增，服务器按原值回应）
    uint16_t type_;          //业务需要时再类型转化
};
#pragma pack(pop)
static_assert(sizeof(PackHeader) == 14,"sizeof PackHeader must be 14");

struct Package{
    uint32_t seq_ = 0;
    PackType type_ = PackType::MESSAGE;
    string body_;
};