#include "hex.hpp"
#include <cassert>
#include <vector>

int main() {
    std::vector<std::uint8_t> bytes;
    assert(openp1::meter::parse_hex("7E a8 00\n", bytes));
    assert((bytes == std::vector<std::uint8_t>{0x7e, 0xa8, 0x00}));
    const auto previous = bytes;
    assert(!openp1::meter::parse_hex("7EX0", bytes));
    assert(bytes == previous);
    assert(!openp1::meter::parse_hex("aabb", bytes, 1));
    assert(bytes == previous);
    assert(!openp1::meter::parse_hex("aa", bytes, 0));
    assert(!openp1::meter::parse_hex("7", bytes));
    assert(!openp1::meter::parse_hex(" ", bytes));
}
