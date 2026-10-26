#pragma once
#include "esp_stub.hpp"
constexpr int ESP_IMAGE_HEADER_MAGIC=0xe9,ESP_CHIP_ID_ESP32=0;
struct esp_image_header_t {std::uint8_t magic=0xe9,segment_count=1,pad[10]{};std::uint16_t chip_id=0;std::uint8_t reserved[9]{},hash_appended=1;};
struct esp_image_segment_header_t {std::uint32_t load_addr=0,data_len=256;};
static_assert(sizeof(esp_image_header_t)==24);
