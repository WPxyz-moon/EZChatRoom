#pragma once
#include <cstdint>
#include <string>
#include <vector>

// FILE_UPLOAD_META / FILE_DOWNLOAD_META 共用
// 上传：客户端发 META 时 fileId 留 0，服务端在 META_ACK 分配
// 下载：服务端发 META 时 fileId 是服务端存储的 id
struct FileMeta {
    uint32_t    fileId = 0;    // 0=未分配（上传时客户端留 0）
    std::string name;          // 文件名
    uint64_t    size = 0;      // 文件总大小
    uint32_t    totalChunks = 0;
    std::string md5;           // 文件内容 MD5（约定 32 字符 hex 小写，或 16 字节原始）
};

// FILE_UPLOAD_META_ACK: S→C 同意/拒绝上传
struct FileUploadMetaAck {
    uint32_t    fileId = 0;    // 服务端分配的 fileId（同意时有效）
    uint16_t    code = 0;      // 0=同意，非0=拒绝（超限/重名等）
    std::string reason;        // 拒绝原因，同意时为空
};

// FILE_UPLOAD_CHUNK / FILE_DOWNLOAD_CHUNK 共用
struct FileChunk {
    uint32_t    fileId = 0;    // 0=未分配
    uint32_t    chunkIndex = 0; // 0=第 0 块（合法块号）
    std::string data;          // 块数据（如 64KB）
};

// FILE_UPLOAD_DONE: C→S 上传完成
struct FileUploadDone {
    uint32_t    fileId = 0;
};

// FILE_UPLOAD_DONE_ACK: S→C 完成确认（含校验结果）
struct FileUploadDoneAck {
    uint32_t    fileId = 0;
    uint16_t    code = 0;      // 0=成功（md5 校验通过），非0=失败（md5 不匹配等）
    std::string reason;
};

// FILE_DOWNLOAD_REQ: C→S 请求下载
struct FileDownloadReq {
    std::string name;          // 要下载的文件名（或 fileId，看存储设计）
};

// FILE_DOWNLOAD_DONE: S→C 下载完成
struct FileDownloadDone {
    uint32_t    fileId = 0;
};

// FILE_LIST_RESP: S→C 服务器上已完成、可下载的文件列表
// 列表来自服务器启动时扫描 <fileId>.meta 重建的索引，按 fileId 倒序（新上传的在前）
struct FileInfo {
    uint32_t    fileId = 0;
    std::string name;
    uint64_t    size = 0;
};

struct FileListResp {
    std::vector<FileInfo> files;
};