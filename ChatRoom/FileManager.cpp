#include "FileManager.h"

#include "Connection.h"
#include "codec.h"
#include "FileMessages.h"
#include "Messages.h"
#include "Md5.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace codec;

namespace {

bool isAllDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

bool isHex32(const std::string& s) {
    if (s.size() != 32) return false;
    for (char c : s) {
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!ok) return false;
    }
    return true;
}

std::string toLower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

}  // namespace

FileManager::FileManager(std::string storageDir) : dir_(std::move(storageDir)) {
    if (dir_.empty()) dir_ = "./uploads";
    if (::mkdir(dir_.c_str(), 0755) < 0 && errno != EEXIST) {
        std::cout << "[WARN] FileManager: cannot create dir " << dir_
                  << " errno=" << std::strerror(errno) << "\n";
    }
    loadIndex();
}

FileManager::~FileManager() {
    cancelAll();
}

std::string FileManager::pathFor(uint32_t id, const char* ext) const {
    return dir_ + "/" + std::to_string(id) + ext;
}

std::string FileManager::sanitizeName(const std::string& name) {
    if (name.empty() || name.size() > 200) return std::string();
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F) return std::string();
        if (c == '/' || c == '\\') return std::string();
        out.push_back(c);
    }
    if (out == "." || out == "..") return std::string();
    return out;
}

// 启动时扫描 <id>.meta 重建索引，重启后文件仍可下载
void FileManager::loadIndex() {
    DIR* dir = ::opendir(dir_.c_str());
    if (!dir) return;

    while (dirent* ent = ::readdir(dir)) {
        std::string fn = ent->d_name;
        if (fn.size() <= 5 || fn.compare(fn.size() - 5, 5, ".meta") != 0) continue;

        errno = 0;
        char* end = nullptr;
        unsigned long idv = std::strtoul(fn.c_str(), &end, 10);
        if (errno != 0 || end == fn.c_str() || idv == 0 || idv > 0xFFFFFFFFul) continue;

        uint32_t id = static_cast<uint32_t>(idv);
        FILE* fp = std::fopen(pathFor(id, ".meta").c_str(), "rb");
        if (!fp) continue;

        FileEntry e;
        e.id = id;
        char line[1024];
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            std::string k = s.substr(0, eq);
            std::string v = s.substr(eq + 1);
            if (k == "name")        e.name = v;
            else if (k == "size")   e.size = std::strtoull(v.c_str(), nullptr, 10);
            else if (k == "chunks") e.totalChunks = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
            else if (k == "md5")    e.md5 = v;
        }
        std::fclose(fp);

        e.name = sanitizeName(e.name);
        if (e.name.empty()) continue;
        e.path = pathFor(id, ".dat");
        if (::access(e.path.c_str(), R_OK) != 0) continue;

        files_[id] = e;
        nameIndex_[e.name] = id;
        if (id >= nextFileId_) nextFileId_ = id + 1;
    }
    ::closedir(dir);
}

bool FileManager::writeMeta(const FileEntry& e) const {
    FILE* fp = std::fopen(pathFor(e.id, ".meta").c_str(), "wb");
    if (!fp) return false;
    std::fprintf(fp, "name=%s\nsize=%llu\nchunks=%u\nmd5=%s\n",
                 e.name.c_str(),
                 static_cast<unsigned long long>(e.size),
                 e.totalChunks,
                 e.md5.c_str());
    return std::fclose(fp) == 0;
}

void FileManager::sendError(Connection* conn, uint16_t code, const std::string& reason) {
    ErrorMsg err;
    err.code = code;
    err.reason = reason;
    conn->send(Package(PackType::ERROR_MSG, encodeErrorMsg(err)));
}

void FileManager::abortUpload(Connection* conn) {
    auto it = uploads_.find(conn);
    if (it == uploads_.end()) return;
    it->second.fp.reset();
    ::unlink(it->second.tmpPath.c_str());
    uploads_.erase(it);
}

void FileManager::abortDownload(Connection* conn) {
    auto it = downloads_.find(conn);
    if (it == downloads_.end()) return;
    conn->setOnWritable(nullptr);
    downloads_.erase(it);
}

void FileManager::cancelConnection(Connection* conn) {
    abortUpload(conn);
    abortDownload(conn);
}

void FileManager::cancelAll() {
    for (auto& kv : uploads_) {
        kv.second.fp.reset();
        ::unlink(kv.second.tmpPath.c_str());
    }
    uploads_.clear();

    // 此时 Connection 必须仍然存活（ChatRoom 析构中先调用本函数再 shutdown）
    for (auto& kv : downloads_) {
        kv.first->setOnWritable(nullptr);
    }
    downloads_.clear();
}

// ===================== 文件列表 =====================

void FileManager::onListReq(Connection* conn) {
    if (conn->getName().empty()) {
        sendError(conn, 2, "Please login first.");
        return;
    }

    // files_ 是启动时由 *.meta 重建、上传完成时增补的索引：
    // 只包含已完成（有 .dat 且 MD5 校验通过）的文件，上传中的 .part 不会出现
    std::vector<FileInfo> list;
    list.reserve(std::min(files_.size(), MAX_LIST_ITEMS));
    for (const auto& kv : files_) {
        FileInfo info;
        info.fileId = kv.second.id;
        info.name = kv.second.name;
        info.size = kv.second.size;
        list.push_back(std::move(info));
    }

    // 新上传的排在前面；超出上限的旧文件不返回（单包受 MAX_BODYLEN 限制）
    std::sort(list.begin(), list.end(), [](const FileInfo& a, const FileInfo& b) {
        return a.fileId > b.fileId;
    });
    if (list.size() > MAX_LIST_ITEMS) list.resize(MAX_LIST_ITEMS);

    FileListResp resp;
    resp.files = std::move(list);
    conn->send(Package(PackType::FILE_LIST_RESP, encodeFileListResp(resp)));
}

// ===================== 上传 =====================

void FileManager::onUploadMeta(Connection* conn, const std::string& body) {
    FileMeta meta;
    if (!decodeFileMeta(body, meta)) {
        sendError(conn, 0, "Bad FILE_UPLOAD_META");
        return;
    }
    if (conn->getName().empty()) {
        sendError(conn, 2, "Please login first.");
        return;
    }

    std::string name = sanitizeName(meta.name);
    if (name.empty()) {
        sendError(conn, 1, "Invalid file name");
        return;
    }
    if (meta.size > MAX_FILE_SIZE) {
        sendError(conn, 1, "File too large");
        return;
    }
    if (!meta.md5.empty() && !isHex32(meta.md5)) {
        sendError(conn, 1, "Invalid md5");
        return;
    }

    uint32_t chunks = static_cast<uint32_t>((meta.size + CHUNK_SIZE - 1) / CHUNK_SIZE);
    if (meta.totalChunks != chunks) {
        sendError(conn, 1, "totalChunks mismatch");
        return;
    }

    // 同一连接重复 META：放弃上一个未完成的上传
    abortUpload(conn);

    uint32_t id = nextFileId_++;
    std::string tmp = pathFor(id, ".part");
    FilePtr fp(std::fopen(tmp.c_str(), "wb"));
    if (!fp) {
        sendError(conn, 2, "Cannot create file on server");
        return;
    }

    UploadSession s;
    s.fileId = id;
    s.name = name;
    s.size = meta.size;
    s.totalChunks = chunks;
    s.expectedMd5 = toLower(meta.md5);
    s.tmpPath = tmp;
    s.fp = std::move(fp);
    uploads_[conn] = std::move(s);

    FileUploadMetaAck ack;
    ack.fileId = id;
    ack.code = 0;
    conn->send(Package(PackType::FILE_UPLOAD_META_ACK, encodeFileUploadMetaAck(ack)));
}

void FileManager::onUploadChunk(Connection* conn, const std::string& body) {
    FileChunk chunk;
    if (!decodeFileChunk(body, chunk)) {
        sendError(conn, 0, "Bad FILE_UPLOAD_CHUNK");
        return;
    }

    auto it = uploads_.find(conn);
    if (it == uploads_.end()) {
        sendError(conn, 1, "No active upload");
        return;
    }
    UploadSession& s = it->second;

    if (chunk.fileId != s.fileId) {
        sendError(conn, 1, "fileId mismatch");
        abortUpload(conn);
        return;
    }
    if (chunk.chunkIndex != s.nextChunk) {
        sendError(conn, 1, "Unexpected chunk index");
        abortUpload(conn);
        return;
    }
    if (chunk.data.empty() || chunk.data.size() > CHUNK_SIZE) {
        sendError(conn, 1, "Invalid chunk size");
        abortUpload(conn);
        return;
    }
    if (s.received + chunk.data.size() > s.size) {
        sendError(conn, 1, "Chunk exceeds declared size");
        abortUpload(conn);
        return;
    }

    size_t n = std::fwrite(chunk.data.data(), 1, chunk.data.size(), s.fp.get());
    if (n != chunk.data.size()) {
        sendError(conn, 2, "Write failed");
        abortUpload(conn);
        return;
    }
    s.received += n;
    ++s.nextChunk;
}

void FileManager::onUploadDone(Connection* conn, const std::string& body) {
    FileUploadDone done;
    if (!decodeFileUploadDone(body, done)) {
        sendError(conn, 0, "Bad FILE_UPLOAD_DONE");
        return;
    }

    auto it = uploads_.find(conn);
    if (it == uploads_.end()) {
        sendError(conn, 1, "No active upload");
        return;
    }
    UploadSession& s = it->second;

    if (done.fileId != s.fileId) {
        sendError(conn, 1, "fileId mismatch");
        abortUpload(conn);
        return;
    }

    FileUploadDoneAck ack;
    ack.fileId = s.fileId;

    if (s.nextChunk != s.totalChunks || s.received != s.size) {
        ack.code = 1;
        ack.reason = "Incomplete upload";
        conn->send(Package(PackType::FILE_UPLOAD_DONE_ACK, encodeFileUploadDoneAck(ack)));
        abortUpload(conn);
        return;
    }

    if (std::fflush(s.fp.get()) != 0 || std::ferror(s.fp.get())) {
        ack.code = 2;
        ack.reason = "Flush failed";
        conn->send(Package(PackType::FILE_UPLOAD_DONE_ACK, encodeFileUploadDoneAck(ack)));
        abortUpload(conn);
        return;
    }
    s.fp.reset();

    std::string actual = md5::fileHexDigest(s.tmpPath);
    if (actual.empty()) {
        ack.code = 2;
        ack.reason = "Checksum failed";
        conn->send(Package(PackType::FILE_UPLOAD_DONE_ACK, encodeFileUploadDoneAck(ack)));
        abortUpload(conn);
        return;
    }
    if (!s.expectedMd5.empty() && s.expectedMd5 != actual) {
        ack.code = 3;
        ack.reason = "MD5 mismatch";
        conn->send(Package(PackType::FILE_UPLOAD_DONE_ACK, encodeFileUploadDoneAck(ack)));
        abortUpload(conn);
        return;
    }

    std::string finalPath = pathFor(s.fileId, ".dat");
    if (std::rename(s.tmpPath.c_str(), finalPath.c_str()) != 0) {
        ack.code = 4;
        ack.reason = "Rename failed";
        conn->send(Package(PackType::FILE_UPLOAD_DONE_ACK, encodeFileUploadDoneAck(ack)));
        abortUpload(conn);
        return;
    }

    FileEntry e;
    e.id = s.fileId;
    e.name = s.name;
    e.size = s.size;
    e.totalChunks = s.totalChunks;
    e.md5 = actual;
    e.path = finalPath;

    files_[e.id] = e;
    nameIndex_[e.name] = e.id;
    if (!writeMeta(e)) {
        std::cout << "[WARN] FileManager: write meta failed for id=" << e.id << "\n";
    }

    ack.code = 0;
    ack.reason.clear();
    conn->send(Package(PackType::FILE_UPLOAD_DONE_ACK, encodeFileUploadDoneAck(ack)));
    uploads_.erase(it);
}

// ===================== 下载 =====================

void FileManager::onDownloadReq(Connection* conn, const std::string& body) {
    FileDownloadReq req;
    if (!decodeFileDownloadReq(body, req)) {
        sendError(conn, 0, "Bad FILE_DOWNLOAD_REQ");
        return;
    }
    if (conn->getName().empty()) {
        sendError(conn, 2, "Please login first.");
        return;
    }
    if (req.name.empty()) {
        sendError(conn, 1, "Empty file name");
        return;
    }

    uint32_t id = 0;
    if (isAllDigits(req.name)) {
        errno = 0;
        unsigned long v = std::strtoul(req.name.c_str(), nullptr, 10);
        if (errno == 0 && v > 0 && v <= 0xFFFFFFFFul) {
            uint32_t cand = static_cast<uint32_t>(v);
            if (files_.count(cand)) id = cand;
        }
    }
    if (id == 0) {
        auto nit = nameIndex_.find(req.name);
        if (nit != nameIndex_.end()) id = nit->second;
    }
    if (id == 0 || files_.count(id) == 0) {
        sendError(conn, 1, "File not found");
        return;
    }

    FileEntry e = files_[id];
    abortDownload(conn);   // 同一连接重复请求：先取消旧下载

    FilePtr fp(std::fopen(e.path.c_str(), "rb"));
    if (!fp) {
        sendError(conn, 2, "Cannot open file on server");
        return;
    }

    FileMeta meta;
    meta.fileId = e.id;
    meta.name = e.name;
    meta.size = e.size;
    meta.totalChunks = e.totalChunks;
    meta.md5 = e.md5;
    conn->send(Package(PackType::FILE_DOWNLOAD_META, encodeFileMeta(meta)));

    DownloadSession s;
    s.fileId = e.id;
    s.nextChunk = 0;
    s.totalChunks = e.totalChunks;
    s.fp = std::move(fp);
    downloads_[conn] = std::move(s);

    // 写缓冲降到低水位后 Connection 会回调这里继续灌数据（背压）
    conn->setOnWritable([this, conn] { pumpDownload(conn); });
    pumpDownload(conn);
}

void FileManager::pumpDownload(Connection* conn) {
    auto it = downloads_.find(conn);
    if (it == downloads_.end()) return;

    DownloadSession& s = it->second;
    if (s.pumping) return;   // send → handleWrite → onWritable 的同步递归，直接返回
    s.pumping = true;

    std::vector<char> buf(CHUNK_SIZE);

    while (s.nextChunk < s.totalChunks && conn->bufferedBytes() < HIGH_WATER) {
        size_t n = std::fread(buf.data(), 1, buf.size(), s.fp.get());
        if (n == 0) {
            conn->setOnWritable(nullptr);
            downloads_.erase(it);
            sendError(conn, 2, "File read error");
            return;
        }

        FileChunk fc;
        fc.fileId = s.fileId;
        fc.chunkIndex = s.nextChunk;
        fc.data.assign(buf.data(), n);
        if (!conn->send(Package(PackType::FILE_DOWNLOAD_CHUNK, encodeFileChunk(fc)))) {
            conn->setOnWritable(nullptr);
            downloads_.erase(it);
            return;
        }
        ++s.nextChunk;
    }

    if (s.nextChunk >= s.totalChunks) {
        uint32_t id = s.fileId;
        conn->setOnWritable(nullptr);
        downloads_.erase(it);   // 关闭文件
        FileDownloadDone done;
        done.fileId = id;
        conn->send(Package(PackType::FILE_DOWNLOAD_DONE, encodeFileDownloadDone(done)));
        return;
    }

    // 写缓冲到水位了：等 Connection 发完并回调 onWritable 再继续
    s.pumping = false;
}
