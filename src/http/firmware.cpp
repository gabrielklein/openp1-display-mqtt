#include "connectivity/internal.hpp"
#include "firmware/update.hpp"
#include "simulation/service.hpp"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include <array>
#include <algorithm>
#include <atomic>
#include "freertos/task.h"
namespace openp1::network::detail {
namespace {
std::atomic_flag uploading=ATOMIC_FLAG_INIT;
struct Upload {esp_ota_handle_t handle=0;bool active=false;~Upload(){if(active)esp_ota_abort(handle);uploading.clear();}};
esp_err_t failure(httpd_req_t* r,const char* status,const char* message){return reply(r,status,message);}
}
esp_err_t firmware_get(httpd_req_t* r){
    if(!permits(2)||!same_origin(r,false))return denied(r);
    const auto* partition=firmware::target();auto* root=cJSON_CreateObject();
    if(!root||!cJSON_AddStringToObject(root,"version",esp_app_get_description()->version)||!cJSON_AddBoolToObject(root,"supported",partition!=nullptr)||!cJSON_AddBoolToObject(root,"pending_verify",firmware::pending_verification())||!cJSON_AddNumberToObject(root,"max_bytes",partition?firmware::maximum_image:0)){cJSON_Delete(root);return reply(r,"503 Service Unavailable","{}");}
    return json(r,root);
}
esp_err_t firmware_put(httpd_req_t* r){
    if(!permits(2)||!same_origin(r,true,"application/octet-stream"))return denied(r);
    const auto* partition=firmware::target();
    if(!partition)return failure(r,"409 Conflict","{\"error\":{\"message\":\"OTA partitions unavailable. Install the OTA layout over USB first.\"}}");
    const auto* boot=esp_ota_get_boot_partition();const auto* running=esp_ota_get_running_partition();
    if(!boot||!running||boot->address!=running->address)return failure(r,"409 Conflict","{\"error\":{\"message\":\"A firmware restart is already pending.\"}}");
    if(firmware::pending_verification())return failure(r,"409 Conflict","{\"error\":{\"message\":\"Current firmware is still being verified. Wait 30 seconds after startup.\"}}");
    constexpr auto prefix_size=sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t)+sizeof(esp_app_desc_t);
    if(r->content_len<=prefix_size||r->content_len>firmware::maximum_image||r->content_len>partition->size)return failure(r,"413 Payload Too Large","{\"error\":{\"message\":\"Invalid firmware length. Upload the application .bin, at most 3 MiB.\"}}");
    if(uploading.test_and_set())return reply(r,"409 Conflict","{}");
    Upload upload;std::array<std::uint8_t,1024> buffer{};std::size_t received=0;const auto deadline=now_ms()+180000;
    auto read=[&](std::size_t wanted)->int{if(!permits(2)||now_ms()>=deadline)return -1;return httpd_req_recv(r,reinterpret_cast<char*>(buffer.data()),wanted);};
    // Collect the descriptor across arbitrary network fragmentation before erasing flash.
    std::array<std::uint8_t,prefix_size> prefix{};
    while(received<prefix.size()){const auto n=read(prefix.size()-received);if(n<=0)return permits(2)?reply(r,"408 Request Timeout","{}"):denied(r);std::memcpy(prefix.data()+received,buffer.data(),n);received+=n;}
    if(!firmware::valid_prefix(prefix.data(),prefix.size(),r->content_len))return failure(r,"400 Bad Request","{\"error\":{\"message\":\"Not a supported ESP32 OpenP1 application image with a date version and SHA256 digest.\"}}");
    if(!permits(2))return denied(r);
    if(now_ms()>=deadline)return reply(r,"408 Request Timeout","{}");
    if(esp_ota_begin(partition,OTA_WITH_SEQUENTIAL_WRITES,&upload.handle)!=ESP_OK)return reply(r,"503 Service Unavailable","{}");
    upload.active=true;
    if(!permits(2))return denied(r);
    if(now_ms()>=deadline)return reply(r,"408 Request Timeout","{}");
    if(esp_ota_write(upload.handle,prefix.data(),prefix.size())!=ESP_OK)return reply(r,"500 Internal Server Error","{}");
    while(received<r->content_len){const auto wanted=std::min(buffer.size(),r->content_len-received);const int n=read(wanted);
        if(n<=0)return permits(2)?reply(r,"408 Request Timeout","{}"):denied(r);
        if(!permits(2))return denied(r);
        if(now_ms()>=deadline)return reply(r,"408 Request Timeout","{}");
        if(esp_ota_write(upload.handle,buffer.data(),n)!=ESP_OK)return reply(r,"500 Internal Server Error","{}");
        received+=n;vTaskDelay(1);
    }
    if(!permits(2))return denied(r);
    if(now_ms()>=deadline)return reply(r,"408 Request Timeout","{}");
    upload.active=false;const auto validated=esp_ota_end(upload.handle);
    if(validated!=ESP_OK)return failure(r,"400 Bad Request","{\"error\":{\"message\":\"Firmware checksum, SHA256 or image validation failed. Running firmware is unchanged.\"}}");
    if(!permits(2))return denied(r);
    if(now_ms()>=deadline)return reply(r,"408 Request Timeout","{}");
    if(esp_ota_set_boot_partition(partition)!=ESP_OK)return reply(r,"500 Internal Server Error","{}");
    const auto result=reply(r,"200 OK","{\"updated\":true,\"restarting\":true}");simulation::request_restart();return result;
}
}
