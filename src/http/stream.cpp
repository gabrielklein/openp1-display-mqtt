#include "support/memory.hpp"
#include "stream.hpp"
#include "connectivity/internal.hpp"
#include "mqtt/payload.hpp"
#include "freertos/task.h"
#include <memory>
#include <new>
namespace openp1::network::detail {
namespace {
std::array<httpd_req_t*,2> streams{};
void close_stream(std::size_t i,httpd_req_t* r){
    httpd_resp_send_chunk(r,nullptr,0);httpd_req_async_handler_complete(r);
    Guard guard;streams[i]=nullptr;
}
void worker(void*){for(;;){stream_tick();vTaskDelay(pdMS_TO_TICKS(1000));}}
}
bool stream_start(){return xTaskCreate(worker,"openp1-events",4096,nullptr,2,nullptr)==pdPASS;}
esp_err_t stream_open(httpd_req_t* r){
    if(!permits(1))return denied(r);
    std::size_t index=streams.size();
    {Guard guard;for(std::size_t i=0;i<streams.size();++i)if(!streams[i]){index=i;break;}}
    if(index==streams.size())return reply(r,"503 Service Unavailable","{}");
    httpd_req_t* copy=nullptr;
    if(httpd_req_async_handler_begin(r,&copy)!=ESP_OK)return reply(r,"503 Service Unavailable","{}");
    httpd_resp_set_type(copy,"text/event-stream");httpd_resp_set_hdr(copy,"Cache-Control","no-store");
    {Guard guard;streams[index]=copy;}return ESP_OK;
}
static void stream_tick_impl(){
    std::array<httpd_req_t*,2> current;{Guard guard;current=streams;}
    if(!current[0]&&!current[1])return;
    if(!permits(1)){for(std::size_t i=0;i<current.size();++i)if(current[i])close_stream(i,current[i]);return;}
    auto s=memory::make<Snapshot>();bool online,provisioning;
    char* data=nullptr;
    if(s && copy_snapshot(*s,online,provisioning)){
        auto* j=mqtt::state_json(*s,now_ms());if(j){data=cJSON_PrintUnformatted(j);cJSON_Delete(j);}
    }
    for(std::size_t i=0;i<current.size();++i)if(current[i]){
        auto* r=current[i];
        if(!data||std::strlen(data)>24576 || !permits(1) ||
            httpd_resp_send_chunk(r,"event: state\ndata: ",HTTPD_RESP_USE_STRLEN)!=ESP_OK || !permits(1) ||
            httpd_resp_send_chunk(r,data,HTTPD_RESP_USE_STRLEN)!=ESP_OK || !permits(1) ||
            httpd_resp_send_chunk(r,"\n\n",2)!=ESP_OK)close_stream(i,r);
    }
    if(data)cJSON_free(data);
}
void stream_tick(){
    try{stream_tick_impl();}
    catch(const std::bad_alloc&){
        std::array<httpd_req_t*,2> current;{Guard guard;current=streams;}
        for(std::size_t i=0;i<current.size();++i)if(current[i])close_stream(i,current[i]);
    }
}
}
