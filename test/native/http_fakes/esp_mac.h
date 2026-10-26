#pragma once
#include "esp_stub.hpp"
constexpr int ESP_MAC_WIFI_STA=0;
inline int esp_read_mac(std::uint8_t* mac,int){for(unsigned i=0;i<6;++i)mac[i]=i+1;return ESP_OK;}
