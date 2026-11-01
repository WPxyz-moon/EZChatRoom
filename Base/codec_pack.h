#include "Package.h"
#include <arpa/inet.h>
#include <cstring>
#include <vector>

namespace codec{
    // ===== 线格式层：字节流 ↔ Package（含粘包/半包） =====
    string serialize(const Package& pkg){
        string out;
        PackHeader pkh;
        pkh.magic_ = htonl(MAGICNUM);
        pkh.seq_ = htonl(pkg.seq_);
        pkh.type_ = htons(static_cast<uint16_t>(pkg.type_));
        pkh.bodyLen_ = htonl(static_cast<uint32_t>(pkg.body_.size()));
        
        out.resize(PACK_HEADER_SIZE + pkg.body_.size());
        memcpy(out.data(), &pkh, PACK_HEADER_SIZE);
        if (!pkg.body_.empty()){
            memcpy(out.data() + PACK_HEADER_SIZE, pkg.body_.data(), pkg.body_.size());
        }
        return out;
    }

    //返回false代表缓冲区数据失效，需要断开连接
    bool parse(string& buffer,vector<Package>& out){
        while(buffer.size() >= PACK_HEADER_SIZE){

            PackHeader pkheader;
            memcpy(&pkheader,buffer.data(),sizeof(pkheader));
            if(ntohl(pkheader.magic_) != MAGICNUM) {
                return false;
            }
            
            uint32_t bodyLen = ntohl(pkheader.bodyLen_);
            if(bodyLen > MAX_BODYLEN){
                return false;
            }

            size_t need = PACK_HEADER_SIZE + static_cast<size_t>(bodyLen);
            if(buffer.size() < need){
                break;
            }

            Package pkg;
            pkg.seq_ = ntohl(pkheader.seq_);
            pkg.type_ = static_cast<PackType>(ntohs(pkheader.type_));
            pkg.body_.assign(buffer.data() + PACK_HEADER_SIZE,bodyLen);

            out.push_back(pkg);
            buffer.erase(0,need);
        }
        return true;
    }
};
