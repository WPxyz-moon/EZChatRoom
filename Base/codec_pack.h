#pragma once
#include "Package.h"
#include <arpa/inet.h>
#include <cstring>
#include <vector>

namespace codec {

// ===== 线格式层：字节流 ↔ Package（含粘包/半包） =====
// 定义写在头文件里，必须 inline，否则多个 TU 包含会重复定义
inline std::string serialize(const Package& pkg) {
    std::string out;
    PackHeader pkh;
    pkh.magic_   = htonl(MAGICNUM);
    pkh.type_    = htons(static_cast<uint16_t>(pkg.type_));
    pkh.bodyLen_ = htonl(static_cast<uint32_t>(pkg.body_.size()));

    out.resize(PACK_HEADER_SIZE + pkg.body_.size());
    memcpy(const_cast<char*>(out.data()), &pkh, PACK_HEADER_SIZE);          // C++17: data() 返回 char*
    if (!pkg.body_.empty()) {
        memcpy(out.data() + PACK_HEADER_SIZE, pkg.body_.data(), pkg.body_.size());
    }
    return out;
}

// 返回 false 代表缓冲区数据失效，需要断开连接
inline bool parse(std::string& buffer, std::vector<Package>& out) {
    size_t offset = 0;
    while (buffer.size() - offset >= PACK_HEADER_SIZE) {
        PackHeader pkheader;
        memcpy(&pkheader, buffer.data() + offset, sizeof(pkheader));

        if (ntohl(pkheader.magic_) != MAGICNUM) return false;

        uint32_t bodyLen = ntohl(pkheader.bodyLen_);
        if (bodyLen > MAX_BODYLEN) return false;

        size_t need = PACK_HEADER_SIZE + static_cast<size_t>(bodyLen);
        if (buffer.size() - offset < need) {
            break;   // 半包，等更多数据
        }

        Package pkg;
        pkg.type_ = static_cast<PackType>(ntohs(pkheader.type_));
        pkg.body_.assign(buffer.data() + offset + PACK_HEADER_SIZE, bodyLen);

        out.push_back(std::move(pkg));
        offset += need;
    }
    
    // 只在循环完全结束后，一次性删除已解析的数据,避免多次O(n)删除
    if (offset > 0) {
        buffer.erase(0, offset);
    }
    return true;
}

}  // namespace codec