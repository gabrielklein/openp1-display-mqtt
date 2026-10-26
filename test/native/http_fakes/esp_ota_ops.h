#pragma once
#include "esp_stub.hpp"
#include <vector>
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_TYPE_APP=0,ESP_PARTITION_SUBTYPE_DATA_OTA=0,ESP_PARTITION_SUBTYPE_APP_FACTORY=0,ESP_PARTITION_SUBTYPE_APP_OTA_0=16,ESP_PARTITION_SUBTYPE_APP_OTA_1=17;
constexpr std::size_t OTA_WITH_SEQUENTIAL_WRITES=0xfffffffe;
struct esp_partition_t {int type,subtype;std::uint32_t address,size;};
using esp_ota_handle_t=unsigned;
enum esp_ota_img_states_t {ESP_OTA_IMG_VALID,ESP_OTA_IMG_PENDING_VERIFY};
inline esp_partition_t fake_factory{0,0,0x10000,0x300000},fake_ota0{0,16,0x320000,0x300000},fake_ota1{0,17,0x620000,0x300000},fake_otadata{1,0,0x310000,0x2000};
inline bool fake_ota_layout=true,fake_ota_begin_ok=true,fake_ota_write_ok=true,fake_ota_end_ok=true,fake_ota_select_ok=true,fake_ota_same_partition=false,fake_ota_state_ok=true;
inline unsigned fake_ota_begins=0,fake_ota_aborts=0,fake_ota_selects=0,fake_ota_confirms=0,fake_ota_rollbacks=0;
inline esp_ota_img_states_t fake_ota_state=ESP_OTA_IMG_VALID;
inline std::vector<std::uint8_t> fake_ota_bytes;
inline const esp_partition_t* esp_partition_find_first(int type,int subtype,const char*){if(!fake_ota_layout)return nullptr;if(type==1)return &fake_otadata;return subtype==16?&fake_ota0:&fake_ota1;}
inline const esp_partition_t* fake_running=&fake_factory;
inline const esp_partition_t* fake_boot=&fake_factory;
inline const esp_partition_t* esp_ota_get_boot_partition(){return fake_boot;}
inline const esp_partition_t* esp_ota_get_running_partition(){return fake_running;}
inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*){return fake_ota_same_partition?&fake_factory:&fake_ota0;}
inline int esp_ota_get_state_partition(const esp_partition_t*,esp_ota_img_states_t* state){*state=fake_ota_state;return fake_ota_state_ok?ESP_OK:ESP_FAIL;}
inline int esp_ota_begin(const esp_partition_t*,std::size_t,unsigned* handle){++fake_ota_begins;fake_ota_bytes.clear();*handle=1;return fake_ota_begin_ok?ESP_OK:ESP_FAIL;}
inline int esp_ota_write(unsigned,const void* bytes,std::size_t size){if(!fake_ota_write_ok)return ESP_FAIL;const auto* p=static_cast<const std::uint8_t*>(bytes);fake_ota_bytes.insert(fake_ota_bytes.end(),p,p+size);return ESP_OK;}
inline void(*fake_after_ota_end)()=nullptr;
inline int esp_ota_end(unsigned){if(fake_after_ota_end)fake_after_ota_end();return fake_ota_end_ok?ESP_OK:ESP_FAIL;}
inline int esp_ota_abort(unsigned){++fake_ota_aborts;return ESP_OK;}
inline int esp_ota_set_boot_partition(const esp_partition_t*){if(!fake_ota_select_ok)return ESP_FAIL;++fake_ota_selects;fake_boot=&fake_ota0;return ESP_OK;}
inline int esp_ota_mark_app_valid_cancel_rollback(){++fake_ota_confirms;fake_ota_state=ESP_OTA_IMG_VALID;return ESP_OK;}
inline int esp_ota_mark_app_invalid_rollback_and_reboot(){++fake_ota_rollbacks;return ESP_OK;}
