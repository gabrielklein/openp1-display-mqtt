#include "serial.hpp"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
namespace openp1::serial {
namespace {
struct Stored {std::uint32_t version=1,baud=115200;std::uint8_t parity=0,stop=1,inverted=1,reserved=0;};
static_assert(sizeof(Stored)==12,"Fixed serial NVS format");
input::SerialSettings saved,initial;
SemaphoreHandle_t mutex=nullptr;
bool ready=false;
}
bool start(const input::SerialSettings& defaults){
    if(!defaults.valid())return false;
    saved=initial=defaults;
    mutex=xSemaphoreCreateMutex();if(!mutex)return false;
    ready=true;
    nvs_handle_t n;const auto opened=nvs_open("openp1",NVS_READONLY,&n);
    if(opened==ESP_ERR_NVS_NOT_FOUND)return true;
    if(opened!=ESP_OK)return false;
    Stored wire;std::size_t size=sizeof(wire);const auto status=nvs_get_blob(n,"serial",&wire,&size);nvs_close(n);
    if(status==ESP_ERR_NVS_NOT_FOUND)return true;
    if(status!=ESP_OK||size!=sizeof(wire)||wire.version!=1||wire.parity>2||wire.stop<1||wire.stop>2||wire.inverted>1||wire.reserved)return false;
    input::SerialSettings candidate;candidate.baud=wire.baud;candidate.parity=static_cast<input::Parity>(wire.parity);
    candidate.stop_bits=wire.stop==2?input::StopBits::Two:input::StopBits::One;candidate.inverted=wire.inverted;
    if(!candidate.valid())return false;
    saved=initial=candidate;return true;
}
bool get(input::SerialSettings& settings){if(!ready)return false;xSemaphoreTake(mutex,portMAX_DELAY);settings=saved;xSemaphoreGive(mutex);return true;}
const input::SerialSettings& boot_settings(){return initial;}
bool configure(const input::SerialSettings& settings){
    if(!ready||!settings.valid())return false;
    Stored wire;wire.baud=settings.baud;wire.parity=static_cast<std::uint8_t>(settings.parity);
    wire.stop=settings.stop_bits==input::StopBits::Two?2:1;wire.inverted=settings.inverted;
    nvs_handle_t n;if(nvs_open("openp1",NVS_READWRITE,&n)!=ESP_OK)return false;
    auto status=nvs_set_blob(n,"serial",&wire,sizeof(wire));if(status==ESP_OK)status=nvs_commit(n);nvs_close(n);
    if(status!=ESP_OK)return false;
    xSemaphoreTake(mutex,portMAX_DELAY);saved=settings;xSemaphoreGive(mutex);return true;
}
}
