#pragma once

namespace openp1::board {
inline constexpr int meter_rx = 27;
inline constexpr int access_button = 35;
inline constexpr int boot_button = 0;
inline constexpr int lcd_mosi = 19;
inline constexpr int lcd_sclk = 18;
inline constexpr int lcd_cs = 5;
inline constexpr int lcd_dc = 16;
inline constexpr int lcd_reset = 23;
inline constexpr int lcd_backlight = 4;
inline constexpr int lcd_width = 240;
inline constexpr int lcd_height = 135;
// GPIO35 has no internal pull-up: verify the board's external button resistor.
}
