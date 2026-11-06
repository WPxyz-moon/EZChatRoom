#include "Md5.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct Ctx {
    uint32_t state[4];
    uint64_t count;       // 累计字节数
    uint8_t  buffer[64];
};

constexpr uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

constexpr uint32_t S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

inline uint32_t rotl(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32 - n));
}

void transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t M[16];
    for (int i = 0; i < 16; ++i) {
        M[i] = static_cast<uint32_t>(block[i * 4]) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];

    for (int i = 0; i < 64; ++i) {
        uint32_t f, g;
        if (i < 16)      { f = (b & c) | (~b & d); g = static_cast<uint32_t>(i); }
        else if (i < 32) { f = (d & b) | (~d & c); g = static_cast<uint32_t>((5 * i + 1) & 15); }
        else if (i < 48) { f = b ^ c ^ d;          g = static_cast<uint32_t>((3 * i + 5) & 15); }
        else             { f = c ^ (b | ~d);       g = static_cast<uint32_t>((7 * i) & 15); }

        uint32_t tmp = d;
        d = c;
        c = b;
        b = b + rotl(a + f + K[i] + M[g], S[i]);
        a = tmp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void update(Ctx& ctx, const uint8_t* data, size_t len) {
    size_t idx = static_cast<size_t>(ctx.count & 63);
    ctx.count += len;
    size_t i = 0;

    if (len >= 64 - idx) {
        size_t part = 64 - idx;
        std::memcpy(ctx.buffer + idx, data, part);
        transform(ctx.state, ctx.buffer);
        i = part;
        for (; i + 64 <= len; i += 64) {
            transform(ctx.state, data + i);
        }
        if (i < len) {
            std::memcpy(ctx.buffer, data + i, len - i);
        }
    } else {
        std::memcpy(ctx.buffer + idx, data, len);
    }
}

std::string digest(Ctx& ctx) {
    uint64_t bits = ctx.count * 8;
    size_t idx = static_cast<size_t>(ctx.count & 63);

    uint8_t pad[64] = {0x80};
    size_t padLen = (idx < 56) ? (56 - idx) : (120 - idx);
    update(ctx, pad, padLen);

    uint8_t lenBuf[8];
    for (int i = 0; i < 8; ++i) {
        lenBuf[i] = static_cast<uint8_t>(bits >> (8 * i));
    }
    update(ctx, lenBuf, 8);

    uint8_t out[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            out[i * 4 + j] = static_cast<uint8_t>((ctx.state[i] >> (8 * j)) & 0xFF);
        }
    }

    static const char* hex = "0123456789abcdef";
    std::string s(32, '0');
    for (int i = 0; i < 16; ++i) {
        s[i * 2]     = hex[out[i] >> 4];
        s[i * 2 + 1] = hex[out[i] & 0x0F];
    }
    return s;
}

}  // namespace

namespace md5 {

std::string hexDigest(const void* data, size_t len) {
    Ctx ctx;
    ctx.state[0] = 0x67452301;
    ctx.state[1] = 0xefcdab89;
    ctx.state[2] = 0x98badcfe;
    ctx.state[3] = 0x10325476;
    ctx.count = 0;

    update(ctx, static_cast<const uint8_t*>(data), len);
    return digest(ctx);
}

std::string hexDigest(const std::string& data) {
    return hexDigest(data.data(), data.size());
}

std::string fileHexDigest(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return std::string();

    Ctx ctx;
    ctx.state[0] = 0x67452301;
    ctx.state[1] = 0xefcdab89;
    ctx.state[2] = 0x98badcfe;
    ctx.state[3] = 0x10325476;
    ctx.count = 0;

    std::vector<uint8_t> buf(64 * 1024);
    for (;;) {
        size_t n = std::fread(buf.data(), 1, buf.size(), fp);
        if (n > 0) update(ctx, buf.data(), n);
        if (n < buf.size()) {
            if (std::ferror(fp)) {
                std::fclose(fp);
                return std::string();
            }
            break;
        }
    }
    std::fclose(fp);
    return digest(ctx);
}

}  // namespace md5
