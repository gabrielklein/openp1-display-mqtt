#pragma once
// Internal shared state: all accesses require Guard, except startup before ready.
// Only bounded reading snapshots cross the acquisition/network boundary.
#include "network.hpp"
#include "access/control.hpp"
#include "cJSON.h"
#include "meter/presentation.hpp"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
namespace openp1::network::detail {
using Snapshot=meter::PresentationSnapshot;
enum class SetupState { Idle, Connecting, Saving, Success, Failed, Timeout, StorageError };
enum class ScanState { Idle, Queued, Running, Ready, Failed };
struct Network { char ssid[33]{}; std::int8_t rssi=0; bool secure=false; };
struct Scan {
    ScanState state=ScanState::Idle;
    std::array<Network,16> networks{};
    std::size_t count=0;
    bool finished=false, successful=false;
    std::uint64_t refresh_after=0, deadline=0;
};
extern SetupState setup_state;
extern Scan scan;
struct RawHistory {
    std::array<input::Chunk,4> chunks{};
    std::array<std::uint64_t,4> sequences{};
    std::size_t count=0;
    std::uint64_t overwritten=0;
};
extern RawHistory raw_history;
extern Snapshot snapshot;
extern SemaphoreHandle_t lock;
extern bool setup, connected, pending, trying, ready;
extern wifi_config_t candidate;
extern std::uint8_t policy;
extern access::Grant grant;
extern unsigned reset_countdown;
extern char ip[16], ap_name[14], csrf[33];
inline std::uint64_t now_ms() { return static_cast<std::uint64_t>(esp_timer_get_time()/1000); }
struct Guard {
    Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
    ~Guard() { xSemaphoreGive(lock); }
};
esp_err_t reply(httpd_req_t* request,const char* code,const char* body,const char* type="application/json");
esp_err_t denied(httpd_req_t* request);
esp_err_t json(httpd_req_t* request,cJSON* root);
esp_err_t admin_get(httpd_req_t* request);
esp_err_t get(httpd_req_t* request);
esp_err_t post(httpd_req_t* request);
esp_err_t put(httpd_req_t* request);
bool permits(std::uint8_t required);
bool same_origin(httpd_req_t* request,bool token,const char* content_type="application/json");
esp_err_t firmware_get(httpd_req_t* request);
esp_err_t firmware_put(httpd_req_t* request);
bool save_policy(std::uint8_t value);
}
