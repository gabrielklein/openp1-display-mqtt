#pragma once
#include <cstdint>
#include "input/pipeline.hpp"
#include <string>

namespace openp1 {
enum class AccessPolicy { None, View, Full };
enum class SourceMode { Uart, Replay };
struct Settings {
    input::SerialSettings serial{};
    AccessPolicy access = AccessPolicy::Full;
    SourceMode source = SourceMode::Uart;
    std::uint32_t temporary_access_seconds = 300;
    std::string mqtt_host;
    std::uint16_t mqtt_port = 1883;
    std::string mqtt_user;
    std::string mqtt_password;
    bool mqtt_tls = false;
    bool home_assistant_discovery = false;
    std::string mqtt_root; // Derive default openp1/<stable-device-id> at runtime.
};
}
