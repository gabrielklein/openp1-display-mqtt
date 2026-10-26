#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace openp1::network {
inline bool read_allowed(bool setup, std::uint8_t policy) {
    return !setup && (policy == 1 || policy == 2);
}
inline bool credentials_valid(const char* ssid, const char* password) {
    const auto s = std::strlen(ssid), p = std::strlen(password);
    return s > 0 && s <= 32 && (p == 0 || (p >= 8 && p <= 63));
}
inline std::uint32_t retry_delay(unsigned failures) {
    return failures >= 6 ? 60000 : (1000u << failures);
}
inline bool stale(std::uint64_t sample, std::uint64_t now) {
    return now >= sample && now - sample >= 15000;
}
// One uncompressed IN/A question only; no arbitrary compression-pointer walks.
inline std::size_t dns_reply(std::uint8_t* b, std::size_t n, std::size_t capacity) {
    if (n > capacity || n < 17 || (b[2] & 0xf8) || b[3] || b[4] || b[5] != 1 ||
        b[6] || b[7] || b[8] || b[9] || b[10] || b[11]) return 0;
    std::size_t i = 12;
    while (i < n && b[i]) {
        const auto len = b[i++];
        if (len > 63 || len > n - i) return 0;
        i += len;
    }
    if (i >= n || i - 12 > 253 || i + 5 != n ||
        b[i+1] || b[i+2] != 1 || b[i+3] || b[i+4] != 1 || capacity - n < 16) return 0;
    const std::uint8_t answer[] = {0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};
    std::memcpy(b+n, answer, sizeof(answer));
    b[2] = 0x81; b[3] = 0x80; b[7] = 1;
    return n + sizeof(answer);
}
}
