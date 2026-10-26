#pragma once
// Host-only transport/mutex doubles. Firmware never includes this directory.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_FAIL=-1, HTTPD_RESP_USE_STRLEN=-1;
constexpr unsigned portMAX_DELAY=0;
using SemaphoreHandle_t=void*;
inline int xSemaphoreTake(SemaphoreHandle_t,unsigned){return 1;}
inline int xSemaphoreGive(SemaphoreHandle_t){return 1;}
extern std::int64_t fake_time_us;
inline std::int64_t esp_timer_get_time(){return fake_time_us;}
struct wifi_config_t { struct { unsigned char ssid[32]{},password[64]{}; } sta; };
struct httpd_req_t {
    const char* uri;
    std::size_t content_len=0,offset=0;
    std::map<std::string,std::string> headers;
    std::string body,response,status,type;
};
inline int httpd_resp_set_status(httpd_req_t* r,const char* s){r->status=s;return ESP_OK;}
inline int httpd_resp_set_type(httpd_req_t* r,const char* s){r->type=s;return ESP_OK;}
inline int httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){return ESP_OK;}
inline int httpd_resp_send(httpd_req_t* r,const char* b,int n){r->response=n<0?std::string(b):std::string(b,static_cast<std::size_t>(n));return ESP_OK;}
inline std::size_t httpd_req_get_hdr_value_len(httpd_req_t* r,const char* key){return r->headers[key].size();}
inline int httpd_req_get_hdr_value_str(httpd_req_t* r,const char* key,char* out,std::size_t capacity){
    const auto& s=r->headers[key];if(s.empty()||s.size()>=capacity)return ESP_FAIL;
    std::memcpy(out,s.c_str(),s.size()+1);return ESP_OK;
}
inline void(*fake_after_recv)()=nullptr;
inline std::size_t fake_recv_limit=static_cast<std::size_t>(-1);
inline int httpd_req_recv(httpd_req_t* r,char* out,std::size_t n){
    if(n>fake_recv_limit)n=fake_recv_limit;
    if(r->offset+n>r->body.size())return ESP_FAIL;
    std::memcpy(out,r->body.data()+r->offset,n);r->offset+=n;if(fake_after_recv)fake_after_recv();return static_cast<int>(n);
}
inline httpd_req_t* last_async_request=nullptr;
inline int completed_streams=0;
inline int httpd_req_async_handler_begin(httpd_req_t* r,httpd_req_t** copy){*copy=new httpd_req_t(*r);last_async_request=*copy;return ESP_OK;}
inline int httpd_req_async_handler_complete(httpd_req_t*){++completed_streams;return ESP_OK;}
inline void(*fake_after_chunk)()=nullptr;
inline int httpd_resp_send_chunk(httpd_req_t* r,const char* data,int n){if(data)r->response.append(data,n<0?std::strlen(data):static_cast<std::size_t>(n));if(fake_after_chunk)fake_after_chunk();return ESP_OK;}
using esp_event_base_t=const char*;
constexpr int ESP_EVENT_ANY_ID=-1;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return reinterpret_cast<void*>(1);}
