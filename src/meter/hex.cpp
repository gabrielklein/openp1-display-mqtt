#include "hex.hpp"

namespace openp1::meter {
namespace {
int digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}
}
bool parse_hex(std::string_view input, std::vector<std::uint8_t>& output, std::size_t max_bytes) {
    std::vector<std::uint8_t> parsed;
    int high = -1;
    for (const char value : input) {
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n') continue;
        const int nibble = digit(value);
        if (nibble < 0) return false;
        if (high < 0) high = nibble;
        else {
            if (parsed.size() >= max_bytes) return false;
            parsed.push_back(static_cast<std::uint8_t>((high << 4) | nibble));
            high = -1;
        }
    }
    if (high >= 0 || parsed.empty()) return false;
    output = parsed;
    return true;
}
}
