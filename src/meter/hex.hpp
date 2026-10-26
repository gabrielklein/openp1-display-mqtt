#pragma once
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <vector>

namespace openp1::meter {
// Accept only a hex payload with optional ASCII whitespace. Log extraction is separate.
bool parse_hex(std::string_view input, std::vector<std::uint8_t>& output,
               std::size_t max_bytes = 16384);
}
