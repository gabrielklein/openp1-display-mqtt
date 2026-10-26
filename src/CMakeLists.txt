idf_component_register(SRCS "main.cpp" "meter/hex.cpp" "meter/hdlc.cpp"
    "meter/axdr.cpp" "meter/processor.cpp" "input/pipeline.cpp" "input/uart.cpp" "input/simulation.cpp" "simulation/service.cpp" "config/serial.cpp" "firmware/update.cpp" "http/firmware.cpp"
    "connectivity/network.cpp" "http/server.cpp" "http/admin.cpp" "http/stream.cpp" "mqtt/service.cpp" "display/view.cpp" "display/st7789.cpp"
    INCLUDE_DIRS "." "../include" REQUIRES app_update esp_app_format bootloader_support driver esp_timer esp_lcd esp_wifi esp_event esp_netif esp_http_server nvs_flash json mqtt mbedtls)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
