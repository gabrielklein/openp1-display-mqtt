#include "update.hpp"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_log.h"
#include <cstring>
namespace openp1::firmware {
bool date_version(const char* value,std::size_t capacity){
    if(!value||capacity<11||!std::memchr(value,0,capacity)||std::strlen(value)!=10||value[4]!='.'||value[7]!='.')return false;
    for(unsigned i=0;i<10;++i)if(i!=4&&i!=7&&(value[i]<'0'||value[i]>'9'))return false;
    const unsigned year=(value[0]-'0')*1000+(value[1]-'0')*100+(value[2]-'0')*10+value[3]-'0',month=(value[5]-'0')*10+value[6]-'0',day=(value[8]-'0')*10+value[9]-'0';
    constexpr unsigned days[]{31,28,31,30,31,30,31,31,30,31,30,31};
    return year>=2000&&month>=1&&month<=12&&day>=1&&day<=days[month-1]+(month==2&&year%4==0&&(year%100!=0||year%400==0)?1:0);
}
bool valid_prefix(const std::uint8_t* bytes,std::size_t size,std::size_t total){
    constexpr auto offset=sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t);
    if(size<offset+sizeof(esp_app_desc_t)||total<offset+sizeof(esp_app_desc_t)||total>maximum_image)return false;
    esp_image_header_t header;esp_image_segment_header_t segment;esp_app_desc_t app;
    std::memcpy(&header,bytes,sizeof(header));std::memcpy(&segment,bytes+sizeof(header),sizeof(segment));std::memcpy(&app,bytes+offset,sizeof(app));
    return header.magic==ESP_IMAGE_HEADER_MAGIC&&header.chip_id==ESP_CHIP_ID_ESP32&&header.segment_count>0&&header.segment_count<=16&&header.hash_appended==1&&segment.data_len>=sizeof(app)&&segment.data_len<=total-offset&&app.magic_word==ESP_APP_DESC_MAGIC_WORD&&std::memchr(app.project_name,0,sizeof(app.project_name))&&!std::strcmp(app.project_name,"openp1_display")&&date_version(app.version,sizeof(app.version));
}
const esp_partition_t* target(){
    if(!esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,nullptr)||!esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,nullptr)||!esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,nullptr))return nullptr;
    const auto* next=esp_ota_get_next_update_partition(nullptr);const auto* running=esp_ota_get_running_partition();
    return next&&running&&next->type==ESP_PARTITION_TYPE_APP&&(next->subtype==ESP_PARTITION_SUBTYPE_APP_OTA_0||next->subtype==ESP_PARTITION_SUBTYPE_APP_OTA_1)&&next->address!=running->address&&next->size>=maximum_image?next:nullptr;
}
bool pending_verification(){esp_ota_img_states_t state;const auto* running=esp_ota_get_running_partition();return running&&esp_ota_get_state_partition(running,&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY;}
void startup_tick(std::uint64_t now,bool network_ready){
    static bool confirmed=false;
    if(confirmed||now<30000)return;
    const auto* running=esp_ota_get_running_partition();if(!running)return;
    if(running->subtype==ESP_PARTITION_SUBTYPE_APP_FACTORY){confirmed=true;return;}
    esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(running,&state)!=ESP_OK)return;
    if(state!=ESP_OTA_IMG_PENDING_VERIFY){confirmed=true;return;}
    if(!network_ready){esp_ota_mark_app_invalid_rollback_and_reboot();return;}
    if(esp_ota_mark_app_valid_cancel_rollback()==ESP_OK){confirmed=true;ESP_LOGI("firmware","Startup checks passed; firmware confirmed");}
    else ESP_LOGW("firmware","Firmware confirmation failed; will retry");
}
}
