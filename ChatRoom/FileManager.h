#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>

// 文件传输会话管理。
//
// 存储布局（dir_ 默认 ./uploads）：
//   <fileId>.part  上传中的临时文件，上传完成/失败时删除
//   <fileId>.dat   已完成的文件内容
//   <fileId>.meta  元信息（文件名/大小/块数/MD5），启动时据此重建索引
//
// 会话按 Connection* 索引，所以 Connection 断开/销毁前必须调用
// cancelConnection()（ChatRoom::removeConnection 里完成）。
class Connection;

class FileManager {
public:
    static constexpr uint32_t CHUNK_SIZE = 64 * 1024;
    static constexpr uint64_t MAX_FILE_SIZE = 512ull * 1024 * 1024;
    // 下载泵：连接写缓冲超过该值就暂停发送，等 onWritable 回调再来补
    static constexpr size_t HIGH_WATER = 1024 * 1024;
    // 列表响应上限：单包不能超过 MAX_BODYLEN(1MB)，超出的旧文件不返回
    static constexpr size_t MAX_LIST_ITEMS = 2000;

    explicit FileManager(std::string storageDir = "./uploads");
    ~FileManager();

    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    // 上传：META 分配 fileId 并创建临时文件；CHUNK 顺序落盘；DONE 校验 MD5 后转正
    void onUploadMeta(Connection* conn, const std::string& body);
    void onUploadChunk(Connection* conn, const std::string& body);
    // 返回 true 表示文件已成功入索引，调用方据此广播文件列表变化
    bool onUploadDone(Connection* conn, const std::string& body);

    // 下载：REQ 后回 META，再按写缓冲水位分块推送 CHUNK，最后 DONE
    void onDownloadReq(Connection* conn, const std::string& body);

    // 列表：把索引里已完成的文件回给客户端（fileId 倒序，新上传的在前）
    void onListReq(Connection* conn);

    // Connection 断开时清掉它的上传/下载会话（由 ChatRoom::removeConnection 调用）
    void cancelConnection(Connection* conn);
    // 进程退出前调用：此时所有 Connection 仍然存活
    void cancelAll();

private:
    struct FileCloser {
        void operator()(FILE* f) const { if (f) std::fclose(f); }
    };
    using FilePtr = std::unique_ptr<FILE, FileCloser>;

    struct FileEntry {
        uint32_t    id = 0;
        std::string name;
        uint64_t    size = 0;
        uint32_t    totalChunks = 0;
        std::string md5;
        std::string path;
    };

    struct UploadSession {
        uint32_t    fileId = 0;
        std::string name;
        uint64_t    size = 0;
        uint32_t    totalChunks = 0;
        std::string expectedMd5;
        std::string tmpPath;
        FilePtr     fp;
        uint32_t    nextChunk = 0;
        uint64_t    received = 0;
    };

    struct DownloadSession {
        uint32_t fileId = 0;
        uint32_t nextChunk = 0;
        uint32_t totalChunks = 0;
        FilePtr  fp;
        bool     pumping = false;   // 防止 send→handleWrite→onWritable 的递归重入
    };

    std::string dir_;
    uint32_t nextFileId_ = 1;
    std::unordered_map<uint32_t, FileEntry>   files_;      // fileId → 元信息
    std::unordered_map<std::string, uint32_t> nameIndex_;  // 文件名 → fileId（同名取最新）
    std::unordered_map<Connection*, UploadSession>   uploads_;
    std::unordered_map<Connection*, DownloadSession> downloads_;

    std::string pathFor(uint32_t id, const char* ext) const;
    void loadIndex();
    bool writeMeta(const FileEntry& e) const;

    void abortUpload(Connection* conn);
    void abortDownload(Connection* conn);
    void pumpDownload(Connection* conn);
    void sendError(Connection* conn, uint16_t code, const std::string& reason);

    // 拒绝空名、控制字符、路径分隔符和 . / ..
    static std::string sanitizeName(const std::string& name);
};
