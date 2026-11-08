#include "codec.h"
#include "Messages.h"
#include "FileMessages.h"
#include "Endian.h"

#include <cstring>

namespace codec {

// ===== 内部辅助：写/读定宽字段（网络序） =====
namespace {

void putU16(std::string& b, uint16_t v) {
    uint16_t n = wire::toBE16(v);
    b.append(reinterpret_cast<const char*>(&n), 2);
}

void putU32(std::string& b, uint32_t v) {
    uint32_t n = wire::toBE32(v);
    b.append(reinterpret_cast<const char*>(&n), 4);
}

void putU64(std::string& b, uint64_t v) {
    uint64_t n = wire::toBE64(v);
    b.append(reinterpret_cast<const char*>(&n), 8);
}

// 变长字符串：uint16_t 长度前缀 + 内容
void putStr16(std::string& b, const std::string& s) {
    if (s.size() > 0xFFFF) return;   // 超长忽略/截断，调用方应保证不超
    putU16(b, static_cast<uint16_t>(s.size()));
    b.append(s);
}

// 变长字符串：uint32_t 长度前缀 + 内容（用于 FileChunk.data）
void putStr32(std::string& b, const std::string& s) {
    putU32(b, static_cast<uint32_t>(s.size()));
    b.append(s);
}

bool getU16(const std::string& b, size_t& off, uint16_t& out) {
    if (b.size() < off + 2) return false;
    uint16_t n;
    std::memcpy(&n, b.data() + off, 2);
    out = wire::fromBE16(n);
    off += 2;
    return true;
}

bool getU32(const std::string& b, size_t& off, uint32_t& out) {
    if (b.size() < off + 4) return false;
    uint32_t n;
    std::memcpy(&n, b.data() + off, 4);
    out = wire::fromBE32(n);
    off += 4;
    return true;
}

bool getU64(const std::string& b, size_t& off, uint64_t& out) {
    if (b.size() < off + 8) return false;
    uint64_t n;
    std::memcpy(&n, b.data() + off, 8);
    out = wire::fromBE64(n);
    off += 8;
    return true;
}

bool getStr16(const std::string& b, size_t& off, std::string& out) {
    uint16_t len;
    if (!getU16(b, off, len)) return false;
    if (b.size() < off + len) return false;
    out.assign(b.data() + off, len);
    off += len;
    return true;
}

bool getStr32(const std::string& b, size_t& off, std::string& out) {
    uint32_t len;
    if (!getU32(b, off, len)) return false;
    if (b.size() < off + len) return false;
    out.assign(b.data() + off, len);
    off += len;
    return true;
}

}  // namespace

// ===== 登录 =====
std::string encodeLogin(const LoginReq& req) {
    std::string body;
    putStr16(body, req.name);
    return body;
}

bool decodeLogin(const std::string& body, LoginReq& out) {
    size_t off = 0;
    return getStr16(body, off, out.name);
}

std::string encodeLoginAck(const LoginAck& ack) {
    std::string body;
    putU16(body, ack.code);
    putStr16(body, ack.reason);
    return body;
}

bool decodeLoginAck(const std::string& body, LoginAck& out) {
    size_t off = 0;
    if (!getU16(body, off, out.code)) return false;
    if (!getStr16(body, off, out.reason)) return false;
    return true;
}

// ===== 聊天 =====
std::string encodeMessage(const Message& msg) {
    std::string body;
    putU64(body, msg.time);
    putStr16(body, msg.sender);
    putStr16(body, msg.text);
    putStr16(body, msg.target);
    return body;
}

bool decodeMessage(const std::string& body, Message& out) {
    size_t off = 0;
    if (!getU64(body, off, out.time)) return false;
    if (!getStr16(body, off, out.sender)) return false;
    if (!getStr16(body, off, out.text)) return false;
    if (!getStr16(body, off, out.target)) return false;
    return true;
}

// ===== 文件元信息（上传/下载共用） =====
std::string encodeFileMeta(const FileMeta& meta) {
    std::string body;
    putU32(body, meta.fileId);
    putStr16(body, meta.name);
    putU64(body, meta.size);
    putU32(body, meta.totalChunks);
    putStr16(body, meta.md5);
    return body;
}

bool decodeFileMeta(const std::string& body, FileMeta& out) {
    size_t off = 0;
    if (!getU32(body, off, out.fileId)) return false;
    if (!getStr16(body, off, out.name)) return false;
    if (!getU64(body, off, out.size)) return false;
    if (!getU32(body, off, out.totalChunks)) return false;
    if (!getStr16(body, off, out.md5)) return false;
    return true;
}

// ===== 上传元信息 ACK =====
std::string encodeFileUploadMetaAck(const FileUploadMetaAck& ack) {
    std::string body;
    putU32(body, ack.fileId);
    putU16(body, ack.code);
    putStr16(body, ack.reason);
    return body;
}

bool decodeFileUploadMetaAck(const std::string& body, FileUploadMetaAck& out) {
    size_t off = 0;
    if (!getU32(body, off, out.fileId)) return false;
    if (!getU16(body, off, out.code)) return false;
    if (!getStr16(body, off, out.reason)) return false;
    return true;
}

// ===== 文件块（上传/下载共用） =====
std::string encodeFileChunk(const FileChunk& chunk) {
    std::string body;
    putU32(body, chunk.fileId);
    putU32(body, chunk.chunkIndex);
    putStr32(body, chunk.data);   // data 可能 64KB，用 uint32_t 长度
    return body;
}

bool decodeFileChunk(const std::string& body, FileChunk& out) {
    size_t off = 0;
    if (!getU32(body, off, out.fileId)) return false;
    if (!getU32(body, off, out.chunkIndex)) return false;
    if (!getStr32(body, off, out.data)) return false;
    return true;
}

// ===== 上传完成 =====
std::string encodeFileUploadDone(const FileUploadDone& done) {
    std::string body;
    putU32(body, done.fileId);
    return body;
}

bool decodeFileUploadDone(const std::string& body, FileUploadDone& out) {
    size_t off = 0;
    return getU32(body, off, out.fileId);
}

// ===== 上传完成 ACK =====
std::string encodeFileUploadDoneAck(const FileUploadDoneAck& ack) {
    std::string body;
    putU32(body, ack.fileId);
    putU16(body, ack.code);
    putStr16(body, ack.reason);
    return body;
}

bool decodeFileUploadDoneAck(const std::string& body, FileUploadDoneAck& out) {
    size_t off = 0;
    if (!getU32(body, off, out.fileId)) return false;
    if (!getU16(body, off, out.code)) return false;
    if (!getStr16(body, off, out.reason)) return false;
    return true;
}

// ===== 下载请求 =====
std::string encodeFileDownloadReq(const FileDownloadReq& req) {
    std::string body;
    putStr16(body, req.name);
    return body;
}

bool decodeFileDownloadReq(const std::string& body, FileDownloadReq& out) {
    size_t off = 0;
    return getStr16(body, off, out.name);
}

// ===== 下载完成 =====
std::string encodeFileDownloadDone(const FileDownloadDone& done) {
    std::string body;
    putU32(body, done.fileId);
    return body;
}

bool decodeFileDownloadDone(const std::string& body, FileDownloadDone& out) {
    size_t off = 0;
    return getU32(body, off, out.fileId);
}

// ===== 文件列表 =====
std::string encodeFileListResp(const FileListResp& resp) {
    std::string body;
    putU32(body, static_cast<uint32_t>(resp.files.size()));
    for (const auto& f : resp.files) {
        putU32(body, f.fileId);
        putStr16(body, f.name);
        putU64(body, f.size);
    }
    return body;
}

bool decodeFileListResp(const std::string& body, FileListResp& out) {
    size_t off = 0;
    uint32_t count = 0;
    if (!getU32(body, off, count)) return false;
    // 一条 body 受 MAX_BODYLEN(1MB) 限制，条目数超过该值必然是坏包
    if (count > MAX_BODYLEN / 8) return false;

    FileListResp resp;
    resp.files.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        FileInfo f;
        if (!getU32(body, off, f.fileId)) return false;
        if (!getStr16(body, off, f.name)) return false;
        if (!getU64(body, off, f.size)) return false;
        resp.files.push_back(std::move(f));
    }
    out = std::move(resp);
    return true;
}

// ===== 通用错误 =====
std::string encodeErrorMsg(const ErrorMsg& err) {
    std::string body;
    putU16(body, err.code);
    putStr16(body, err.reason);
    return body;
}

bool decodeErrorMsg(const std::string& body, ErrorMsg& out) {
    size_t off = 0;
    if (!getU16(body, off, out.code)) return false;
    if (!getStr16(body, off, out.reason)) return false;
    return true;
}

}  // namespace codec