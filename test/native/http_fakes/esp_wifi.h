#pragma once
#include "esp_stub.hpp"

struct wifi_ap_record_t {std::int8_t rssi=-62;};
inline bool fake_ap_info_ok=true;
inline int esp_wifi_sta_get_ap_info(wifi_ap_record_t* out){out->rssi=-62;return fake_ap_info_ok?ESP_OK:ESP_FAIL;}
