#pragma once
#include <atomic>
#include <cstdint>
namespace openp1::dashboard {
inline constexpr std::uint32_t default_limit_w=5000;
inline std::atomic<std::uint32_t> limit_w{default_limit_w};
inline bool valid_limit(std::uint32_t value){return value>=100&&value<=1000000;}
}
