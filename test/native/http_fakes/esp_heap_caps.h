#pragma once
#include <cstddef>
constexpr unsigned MALLOC_CAP_8BIT=1;
inline std::size_t heap_caps_get_free_size(unsigned){return 65536;}
inline std::size_t heap_caps_get_largest_free_block(unsigned){return 32768;}
inline std::size_t heap_caps_get_minimum_free_size(unsigned){return 16384;}
