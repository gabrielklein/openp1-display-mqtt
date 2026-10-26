#pragma once
#include "esp_stub.hpp"
#define ESP_APP_DESC_MAGIC_WORD 0xABCD5432
struct esp_app_desc_t {std::uint32_t magic_word=ESP_APP_DESC_MAGIC_WORD,secure_version=0,reserv1[2]{};char version[32]="2026.10.26",project_name[32]="openp1_display",time[16]{},date[16]{},idf_ver[32]{};std::uint8_t app_elf_sha256[32]{};std::uint32_t reserv2[20]{};};
static_assert(sizeof(esp_app_desc_t)==256);
inline const esp_app_desc_t* esp_app_get_description(){static esp_app_desc_t app;return &app;}
