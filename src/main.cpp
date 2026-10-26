#include <cstdio>
#include <cstring>
#include <new>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#if !CONFIG_COMPILER_CXX_EXCEPTIONS
#error "Enable CONFIG_COMPILER_CXX_EXCEPTIONS for allocation failure recovery"
#endif
#if !defined(CONFIG_COMPILER_CXX_EXCEPTIONS_EMG_POOL_SIZE) || CONFIG_COMPILER_CXX_EXCEPTIONS_EMG_POOL_SIZE < 2048
#error "Reserve at least 2048 bytes for the C++ exception emergency pool"
#endif
#if !defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) || !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "Enable CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE for the OTA firmware profile"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config/serial.hpp"
#include "firmware/update.hpp"
#include "esp_app_desc.h"
#include "input/uart.hpp"
#include "display/view.hpp"
#include "connectivity/network.hpp"
#include "mqtt/service.hpp"
#include "simulation/service.hpp"
#include "nvs_flash.h"

namespace {
bool network_ready=false;
void update_display(const openp1::input::Pipeline& pipeline, std::uint64_t now) {
    static std::uint64_t previous = 0;
    static bool first = true;
    if (!first && now - previous < 250) return;
    first = false; previous = now;
    openp1::firmware::startup_tick(now,network_ready);
    const bool simulation = pipeline.source() == openp1::input::Source::Replay;
    openp1::network::publish(pipeline, now);
    auto view = openp1::display::make_view(pipeline.processor(), simulation, now);
    openp1::network::decorate(view, now);
    static openp1::display::ReceptionPulse reception;
    reception.update(pipeline.processor().counters().notifications,now);view.receiving=reception.active(now);
    const auto mqtt=openp1::mqtt::statistics();const auto* status=openp1::mqtt::status();
    view.mqtt=mqtt.connected?openp1::display::MqttState::Connected:!std::strcmp(status,"disabled")?openp1::display::MqttState::Disabled:!std::strcmp(status,"error")?openp1::display::MqttState::Error:openp1::display::MqttState::Waiting;
    openp1::display::submit(view);
}
}

extern "C" void app_main() {
    static openp1::input::Pipeline pipeline;
    using namespace openp1::input;
    SerialSettings settings;
    settings.baud = CONFIG_OPENP1_BAUD;
#ifdef CONFIG_OPENP1_PARITY_EVEN
    settings.parity = Parity::Even;
#elif defined(CONFIG_OPENP1_PARITY_ODD)
    settings.parity = Parity::Odd;
#endif
#ifdef CONFIG_OPENP1_TWO_STOP_BITS
    settings.stop_bits = StopBits::Two;
#endif
#ifdef CONFIG_OPENP1_INVERT_RX
    settings.inverted = true;
#else
    settings.inverted = false;
#endif
    // Initialize exception bookkeeping before Wi-Fi/MQTT consume the heap.
    // ESP-IDF's emergency-pool test performs the same initial throw/catch.
    try{throw std::bad_alloc();}catch(const std::bad_alloc&){}
    if(nvs_flash_init()!=ESP_OK || !openp1::simulation::start())
        ESP_LOGE("openp1","Simulation settings unavailable; using UART");
    if(!openp1::serial::start(settings))ESP_LOGE("openp1","Saved serial settings unavailable; using board-profile defaults");
    const auto& serial_settings=openp1::serial::boot_settings();
    const auto& simulation=openp1::simulation::boot_data();
    pipeline.select(simulation.enabled?Source::Replay:Source::Uart);
    pipeline.observe(
        [&](const std::uint8_t* body,std::size_t size,std::uint64_t){openp1::mqtt::wire(body,size,pipeline.source());},
        [&](const openp1::meter::Bytes& bytes,std::uint64_t){openp1::mqtt::apdu(bytes,pipeline.source());});
    openp1::display::start();
    network_ready=openp1::network::start(pipeline.source());
    if (!network_ready) ESP_LOGE("openp1", "Wi-Fi initialization failed; acquisition continues");
    if(!openp1::mqtt::start())ESP_LOGE("openp1","MQTT initialization failed; acquisition continues");
    ESP_LOGI("openp1","RAM free %u bytes; largest 8-bit block %u bytes",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    ESP_LOGI("openp1", "Firmware %s; meter electrical compatibility unverified",esp_app_get_description()->version);
    if(simulation.enabled){
        SimulationPlayer player(simulation);
        ESP_LOGW("openp1","SIMULATION: UART disabled; using saved settings");
        for(;;){
            const auto now=static_cast<std::uint64_t>(esp_timer_get_time()/1000);
            if(player.tick(pipeline,now))ESP_LOGI("openp1","Replay complete; repeating in 3.5 seconds");
            pipeline.tick(now);update_display(pipeline,now);vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    ESP_LOGI("openp1", "UART1 RX GPIO27, baud %lu, parity %d, stop %d, inverted %d",
             static_cast<unsigned long>(serial_settings.baud),
             static_cast<int>(serial_settings.parity),
             serial_settings.stop_bits == StopBits::Two ? 2 : 1, serial_settings.inverted);
    update_display(pipeline, static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
    run_uart(pipeline, serial_settings, update_display);
}
