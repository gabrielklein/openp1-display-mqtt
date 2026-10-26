#pragma once
#include "esp_stub.hpp"
struct esp_sntp_config_t{const char* server;};
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(s) esp_sntp_config_t{s}
inline unsigned fake_ntp_calls=0;
inline int esp_netif_sntp_init(const esp_sntp_config_t*){++fake_ntp_calls;return ESP_OK;}
inline void esp_netif_sntp_deinit(){}
