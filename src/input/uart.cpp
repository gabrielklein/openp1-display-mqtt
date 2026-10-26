#include "uart.hpp"
#include "board.hpp"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace openp1::input {
void run_uart(Pipeline& pipeline, const SerialSettings& settings, Update update) {
    if (!settings.valid()) { ESP_LOGE("input", "Invalid serial settings"); return; }
    uart_config_t config{};
    config.baud_rate = static_cast<int>(settings.baud);
    config.data_bits = UART_DATA_8_BITS;
    config.parity = settings.parity == Parity::Even ? UART_PARITY_EVEN :
                    settings.parity == Parity::Odd ? UART_PARITY_ODD : UART_PARITY_DISABLE;
    config.stop_bits = settings.stop_bits == StopBits::Two ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    constexpr auto port = UART_NUM_1;
    QueueHandle_t events = nullptr;
    ESP_ERROR_CHECK(uart_param_config(port, &config));
    ESP_ERROR_CHECK(uart_set_pin(port, UART_PIN_NO_CHANGE, board::meter_rx,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_line_inverse(port, settings.inverted ? UART_SIGNAL_RXD_INV : 0));
    ESP_ERROR_CHECK(uart_driver_install(port, 4096, 0, 32, &events, 0));
    std::array<std::uint8_t, 256> bytes{};
    for (;;) {
        uart_event_t event{};
        if (xQueueReceive(events, &event, pdMS_TO_TICKS(20))) {
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
                event.type == UART_PARITY_ERR || event.type == UART_FRAME_ERR ||
                event.type == UART_BREAK) {
                pipeline.uart_error(static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
                ESP_LOGW("input", "UART error %d; raw history retained", static_cast<int>(event.type));
            }
        }
        // Drain even if a data event was lost; never flush buffered bytes on errors.
        const int count = uart_read_bytes(port, bytes.data(), bytes.size(), 0);
        if (count > 0) pipeline.ingest(Source::Uart, Layer::WireHdlc, bytes.data(),
            static_cast<std::size_t>(count), static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
        const auto now = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
        pipeline.tick(now);
        if (update) update(pipeline, now);
    }
}
}
