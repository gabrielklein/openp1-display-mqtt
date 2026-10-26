#include "service.hpp"
#include <atomic>
#include "esp_timer.h"
#include "support/memory.hpp"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
namespace openp1::simulation {
namespace {
input::SimulationConfig stored;
input::SimulationData initial;
SemaphoreHandle_t mutex=nullptr;
std::atomic<std::uint32_t> restart_at{0};
std::atomic<bool> restart_pending{false};
bool ready=false;
}
bool start(){
    mutex=xSemaphoreCreateMutex();if(!mutex)return false;
    nvs_handle_t n;const auto opened=nvs_open("openp1",NVS_READONLY,&n);
    if(opened==ESP_OK){
        std::size_t size=sizeof(stored);const auto status=nvs_get_blob(n,"simulation",&stored,&size);nvs_close(n);
        if(status!=ESP_ERR_NVS_NOT_FOUND&&(status!=ESP_OK||size!=sizeof(stored))){
            ESP_LOGE("simulation","Invalid saved simulation blob; using UART defaults");stored.reset();
        }
    }else if(opened!=ESP_ERR_NVS_NOT_FOUND)return false;
    char error[128];
    if(!input::prepare_simulation(stored,initial,error,sizeof(error))){
        ESP_LOGE("simulation","Invalid saved simulation settings; using UART defaults");
        stored.reset();
        if(!input::prepare_simulation(stored,initial,error,sizeof(error)))return false;
    }
    ready=true;return true;
}
bool get(input::SimulationConfig& config){
    if(!ready)return false;
    xSemaphoreTake(mutex,portMAX_DELAY);config=stored;xSemaphoreGive(mutex);return true;
}
const input::SimulationData& boot_data(){return initial;}
bool configure(const input::SimulationConfig& config,char* error,std::size_t capacity){
    if(!ready)return false;
    auto prepared=memory::make<input::SimulationData>();if(!prepared)return false;
    if(!input::prepare_simulation(config,*prepared,error,capacity))return false;
    nvs_handle_t n;if(nvs_open("openp1",NVS_READWRITE,&n)!=ESP_OK)return false;
    auto status=nvs_set_blob(n,"simulation",&config,sizeof(config));if(status==ESP_OK)status=nvs_commit(n);nvs_close(n);
    if(status!=ESP_OK)return false;
    xSemaphoreTake(mutex,portMAX_DELAY);stored=config;xSemaphoreGive(mutex);
    return true;
}
void request_restart(){
    restart_at=static_cast<std::uint32_t>(esp_timer_get_time()/1000)+1500;restart_pending=true;
}
bool restart_due(std::uint64_t now){return restart_pending&&static_cast<std::int32_t>(static_cast<std::uint32_t>(now)-restart_at.load())>=0;}
}
