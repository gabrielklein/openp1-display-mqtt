#include "view.hpp"
#include <algorithm>
#include <atomic>
#include "board.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace openp1::display {
namespace {
QueueHandle_t mailbox = nullptr;
std::atomic<bool> rotate_180{false};
#ifdef CONFIG_OPENP1_DISPLAY
SemaphoreHandle_t done = nullptr;
esp_lcd_panel_handle_t panel = nullptr;
esp_lcd_panel_io_handle_t io = nullptr;
bool transferred(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
    BaseType_t awake = pdFALSE;
    xSemaphoreGiveFromISR(done, &awake);
    return awake == pdTRUE;
}
void worker(void*) {
    // Driver and panel operations never run in the acquisition task.
    auto* pixels = static_cast<std::uint16_t*>(heap_caps_malloc(240 * transfer_rows * 2, MALLOC_CAP_DMA));
    auto checked = [](esp_err_t status) {
        if (status != ESP_OK) ESP_LOGE("display", "%s", esp_err_to_name(status));
        return status == ESP_OK;
    };
    bool bus_ready = false;
    do {
        if (!pixels) { ESP_LOGE("display", "Framebuffer allocation failed"); break; }
        spi_bus_config_t bus{};
        bus.mosi_io_num = board::lcd_mosi; bus.sclk_io_num = board::lcd_sclk;
        bus.miso_io_num = -1; bus.quadwp_io_num = -1; bus.quadhd_io_num = -1;
        bus.max_transfer_sz = 240 * transfer_rows * 2;
        if (!checked(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO))) break;
        bus_ready = true;
        esp_lcd_panel_io_spi_config_t transport{};
        transport.dc_gpio_num = board::lcd_dc; transport.cs_gpio_num = board::lcd_cs;
        transport.pclk_hz = 20000000; transport.lcd_cmd_bits = 8; transport.lcd_param_bits = 8;
        transport.spi_mode = 0; transport.trans_queue_depth = 1;
        transport.on_color_trans_done = transferred;
        if (!checked(esp_lcd_new_panel_io_spi(SPI2_HOST, &transport, &io))) break;
        esp_lcd_panel_dev_config_t config{};
        config.reset_gpio_num = board::lcd_reset;
        config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; config.bits_per_pixel = 16;
        if (!checked(esp_lcd_new_panel_st7789(io, &config, &panel)) ||
            !checked(esp_lcd_panel_reset(panel)) || !checked(esp_lcd_panel_init(panel)) ||
            !checked(esp_lcd_panel_invert_color(panel, true)) ||
            !checked(esp_lcd_panel_swap_xy(panel, true)) ||
            !checked(esp_lcd_panel_mirror(panel, true, false)) ||
            !checked(esp_lcd_panel_set_gap(panel, 40, 53)) ||
            !checked(esp_lcd_panel_disp_on_off(panel, true)) ||
            !checked(gpio_set_direction(static_cast<gpio_num_t>(board::lcd_backlight), GPIO_MODE_OUTPUT)) ||
            !checked(gpio_set_level(static_cast<gpio_num_t>(board::lcd_backlight), 1))) break;
        for (;;) {
            View view;
            if (xQueueReceive(mailbox, &view, portMAX_DELAY)) {
                const bool rotate=rotate_180.load();
                bool healthy=true;
                for(std::size_t first=0;first<135;first+=transfer_rows){
                    const auto rows=std::min(transfer_rows,135-first);
                    render_rows(view,pixels,first,rows,rotate);
                    if(!checked(esp_lcd_panel_draw_bitmap(panel,0,first,240,first+rows,pixels))){healthy=false;break;}
                    // DMA must finish before the same band buffer is rendered again.
                    xSemaphoreTake(done,portMAX_DELAY);
                }
                if(!healthy)break;
            }
        }
    } while (false);
    if (panel) { esp_lcd_panel_del(panel); panel = nullptr; }
    if (io) { esp_lcd_panel_io_del(io); io = nullptr; }
    if (bus_ready) spi_bus_free(SPI2_HOST);
    heap_caps_free(pixels);
    // Keep mailbox allocated: acquisition may still overwrite it safely after failure.
    vTaskDelete(nullptr);
}
#endif
}
bool rotated() { return rotate_180.load(); }
void set_rotated(bool enabled) { rotate_180.store(enabled); }
bool start() {
#ifdef CONFIG_OPENP1_DISPLAY
    if (mailbox) return true;
    mailbox = xQueueCreate(1, sizeof(View));
    done = xSemaphoreCreateBinary();
    if (mailbox && done && xTaskCreate(worker, "st7789", 4096, nullptr, 1, nullptr) == pdPASS) return true;
    if (mailbox) vQueueDelete(mailbox);
    if (done) vSemaphoreDelete(done);
    mailbox = nullptr; done = nullptr;
    ESP_LOGE("display", "Display task creation failed; acquisition continues");
#endif
    return false;
}
void submit(const View& view) {
    if (mailbox) xQueueOverwrite(mailbox, &view);
}
}
