#pragma once
#include <cstdint>

// 可移植的网络字节序（大端）读写辅助。
//
// 替代 Linux 专用的 <arpa/inet.h>（htonl/htons/ntohl/ntohs）和
// <endian.h>（htobe64/be64toh），只依赖 <cstdint>，
// Linux（gcc/clang）和 Windows（MinGW/MSVC）都能编译。
//
// toBE*/fromBE* 在主机为大端时是恒等变换，小端时做字节翻转。
// 两者互为逆运算，名字分开只是为了表达意图。
namespace wire {

constexpr std::uint16_t bswap16(std::uint16_t v) noexcept {
    return static_cast<std::uint16_t>((v << 8) | (v >> 8));
}

constexpr std::uint32_t bswap32(std::uint32_t v) noexcept {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}

constexpr std::uint64_t bswap64(std::uint64_t v) noexcept {
    return (static_cast<std::uint64_t>(bswap32(static_cast<std::uint32_t>(v))) << 32) |
           static_cast<std::uint64_t>(bswap32(static_cast<std::uint32_t>(v >> 32)));
}

// GCC/Clang 在所有目标上都定义了 __BYTE_ORDER__；MSVC 支持的架构都是小端。
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
constexpr bool kHostBigEndian = true;
#else
constexpr bool kHostBigEndian = false;
#endif

constexpr std::uint16_t toBE16(std::uint16_t v) noexcept {
    return kHostBigEndian ? v : bswap16(v);
}
constexpr std::uint32_t toBE32(std::uint32_t v) noexcept {
    return kHostBigEndian ? v : bswap32(v);
}
constexpr std::uint64_t toBE64(std::uint64_t v) noexcept {
    return kHostBigEndian ? v : bswap64(v);
}

constexpr std::uint16_t fromBE16(std::uint16_t v) noexcept { return toBE16(v); }
constexpr std::uint32_t fromBE32(std::uint32_t v) noexcept { return toBE32(v); }
constexpr std::uint64_t fromBE64(std::uint64_t v) noexcept { return toBE64(v); }

}  // namespace wire
