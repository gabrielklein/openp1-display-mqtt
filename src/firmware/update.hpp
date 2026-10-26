#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_ota_ops.h"
namespace openp1::firmware {
inline constexpr std::size_t maximum_image=0x300000;
bool date_version(const char* value,std::size_t capacity);
bool valid_prefix(const std::uint8_t* bytes,std::size_t size,std::size_t total);
const esp_partition_t* target();
bool pending_verification();
// Called only by acquisition after initialization, never by HTTP.
void startup_tick(std::uint64_t now,bool network_ready);
}
