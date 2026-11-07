#pragma once

#include "Package.h"

#include <string>
#include <vector>

// ===== body 结构前置声明 =====
// 不 include 具体定义，避免 Connection include codec.h 时拖入所有业务类型。
// codec.cpp 里才 include 这些结构的完整定义。
struct LoginReq;
struct LoginAck;
struct Message;
struct ErrorMsg;
struct FileMeta;
struct FileUploadMetaAck;
struct FileChunk;
struct FileUploadDone;
struct FileUploadDoneAck;
struct FileDownloadReq;
struct FileDownloadDone;

// ===== body 层：Package.body ↔ 具体结构 =====
// 每个 encode 返回 Package 的 body 字节流；decode 从 body 还原结构
// decode 返回 false = body 格式错，调用方回 ERROR_MSG，不断连
namespace codec {

// ===== 登录 =====
std::string encodeLogin(const LoginReq& req);
bool decodeLogin(const std::string& body, LoginReq& out);

std::string encodeLoginAck(const LoginAck& ack);
bool decodeLoginAck(const std::string& body, LoginAck& out);

// ===== 聊天 =====
std::string encodeMessage(const Message& msg);
bool decodeMessage(const std::string& body, Message& out);

// ===== 文件元信息（上传/下载共用） =====
std::string encodeFileMeta(const FileMeta& meta);
bool decodeFileMeta(const std::string& body, FileMeta& out);

// ===== 上传元信息 ACK =====
std::string encodeFileUploadMetaAck(const FileUploadMetaAck& ack);
bool decodeFileUploadMetaAck(const std::string& body, FileUploadMetaAck& out);

// ===== 文件块（上传/下载共用） =====
std::string encodeFileChunk(const FileChunk& chunk);
bool decodeFileChunk(const std::string& body, FileChunk& out);

// ===== 上传完成 =====
std::string encodeFileUploadDone(const FileUploadDone& done);
bool decodeFileUploadDone(const std::string& body, FileUploadDone& out);

// ===== 上传完成 ACK =====
std::string encodeFileUploadDoneAck(const FileUploadDoneAck& ack);
bool decodeFileUploadDoneAck(const std::string& body, FileUploadDoneAck& out);

// ===== 下载请求 =====
std::string encodeFileDownloadReq(const FileDownloadReq& req);
bool decodeFileDownloadReq(const std::string& body, FileDownloadReq& out);

// ===== 下载完成 =====
std::string encodeFileDownloadDone(const FileDownloadDone& done);
bool decodeFileDownloadDone(const std::string& body, FileDownloadDone& out);

// ===== 通用错误 =====
std::string encodeErrorMsg(const ErrorMsg& err);
bool decodeErrorMsg(const std::string& body, ErrorMsg& out);

}  // namespace codec