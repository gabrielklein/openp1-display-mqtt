#pragma once
#include "pipeline.hpp"
namespace openp1::input {
// Runs in app_main's task; UART1 leaves USB UART0 available.
using Update = void (*)(const Pipeline&, std::uint64_t);
void run_uart(Pipeline& pipeline, const SerialSettings& settings, Update update = nullptr);
}
