#pragma once
#include "input/pipeline.hpp"
namespace openp1::serial {
// Initialize before network tasks; defaults come from the existing board profile.
bool start(const input::SerialSettings& defaults);
bool get(input::SerialSettings& settings);
const input::SerialSettings& boot_settings();
bool configure(const input::SerialSettings& settings);
}
