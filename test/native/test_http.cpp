#include "firmware/update.hpp"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "config/serial.hpp"
#include "display/dashboard.hpp"
#include "connectivity/internal.hpp"
#include "cJSON.h"
#include "nvs.h"
#include "meter/hex.hpp"
#include "http/stream.hpp"
#include "mqtt/service.hpp"
#include "simulation/service.hpp"
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstring>
std::int64_t fake_time_us=1000000;
extern "C" void* __real_malloc(std::size_t);
static bool throw_next_new=false;
void* operator new(std::size_t size){if(throw_next_new){throw_next_new=false;throw std::bad_alloc();}if(void* p=__real_malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
static std::size_t reject_allocation_size=0,reject_allocation_at_least=0,rejected_allocations=0;
extern "C" void* __wrap_malloc(std::size_t size){
    if((reject_allocation_size&&size==reject_allocation_size)||(reject_allocation_at_least&&size>=reject_allocation_at_least)){
        ++rejected_allocations;return nullptr;
    }
    return __real_malloc(size);
}
namespace openp1::network::detail {
Snapshot snapshot;
RawHistory raw_history;
SemaphoreHandle_t lock=nullptr;
bool setup=true,connected=false,pending=false,trying=false,ready=true,policy_save_ok=true;
wifi_config_t candidate{};
std::uint8_t policy=1;
openp1::access::Grant grant;
unsigned reset_countdown=0;
char ip[16]="192.168.1.42",ap_name[14]="OpenP1-ABCDEF",csrf[33]="test-token";
SetupState setup_state=SetupState::Idle;
Scan scan;
}
namespace openp1::network {
bool copy_snapshot(meter::PresentationSnapshot& out,bool& station,bool& provisioning){
    out=detail::snapshot;station=detail::connected;provisioning=detail::setup;return true;
}
namespace detail {bool save_policy(std::uint8_t value){if(!policy_save_ok)return false;policy=value;return true;}}
}
namespace openp1::display {
bool rotation=false;
bool rotated(){return rotation;}
void set_rotated(bool enabled){rotation=enabled;}
}
namespace openp1::mqtt {
Config fixture;
bool get(Config& c){c=fixture;return true;}
bool configure(const Config& c){fixture=c;return true;}
const char* status(){return "test";}
Statistics statistics(){Statistics s;s.connected=true;s.sent=42;return s;}
const char* error_name(Failure){return "None";}
std::uint32_t text_dropped(){return 0;}
}
using namespace openp1::network::detail;
httpd_req_t request(const char* path) {
    httpd_req_t r{};r.uri=path;r.headers={{"Host","192.168.4.1"},{"Origin","http://192.168.4.1"},
        {"X-Setup-Token","test-token"},{"Content-Type","application/json"}};return r;
}
int main() {
    assert(openp1::simulation::start());
    assert(openp1::serial::start(openp1::input::SerialSettings{}));
    auto r=request("/setup/token");r.headers["Host"]="captive.example";get(&r);
    assert(r.status=="302 Found" && r.response.find("test-token")==std::string::npos);
    r=request("/setup/scan");assert(get(&r)==ESP_OK && r.status=="202 Accepted");
    assert(scan.state==ScanState::Queued);
    scan.state=ScanState::Running;r=request("/setup/scan");get(&r);assert(r.status=="202 Accepted");
    scan.state=ScanState::Ready;scan.count=1;std::strcpy(scan.networks[0].ssid,"<untrusted>");
    r=request("/setup/scan");get(&r);assert(r.status=="200 OK");
    auto* root=cJSON_Parse(r.response.c_str());assert(root);
    auto* names=cJSON_GetObjectItemCaseSensitive(root,"networks");assert(cJSON_GetArraySize(names)==1);cJSON_Delete(root);
    scan.state=ScanState::Failed;r=request("/setup/scan");get(&r);assert(r.status=="503 Service Unavailable");
    trying=true;r=request("/setup/scan");get(&r);assert(r.status=="409 Conflict");trying=false;
    for(auto state:{SetupState::Idle,SetupState::Connecting,SetupState::Saving,SetupState::Success,SetupState::Failed,SetupState::Timeout,SetupState::StorageError}){
        setup_state=state;std::strcpy(reinterpret_cast<char*>(candidate.sta.password),"never-export-this");
        r=request("/setup/status");get(&r);assert(r.status=="200 OK");
        assert(r.response.find("never-export-this")==std::string::npos);
        assert((r.response.find("192.168.1.42")!=std::string::npos)==(state==SetupState::Success));
    }
    // Setup, including success grace time, never exposes meter data under any policy.
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/values");get(&r);assert(r.status=="403 Forbidden");}
    r=request("/access-check");get(&r);assert(r.status=="403 Forbidden"&&r.response.empty());
    setup=false;
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/values");get(&r);assert(r.status==(p?"200 OK":"403 Forbidden"));}
    r=request("/setup/status");get(&r);assert(r.status=="404 Not Found");
    for(std::uint8_t p=0;p<3;++p){policy=p;grant.until=0;
        r=request("/access-check");get(&r);assert(r.status==(p==2?"204 No Content":"403 Forbidden")&&r.response.empty());
        r=request("/unlock");get(&r);assert(r.status=="200 OK"&&r.response.find("Press S2 to unlock")!=std::string::npos);
        r=request("/settings");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);assert(r.status==(p==2?"200 OK":"403 Forbidden"));
        if(p!=2)assert(r.response.find("Press S2 to unlock")!=std::string::npos);
    }
    policy=0;r=request("/");get(&r);assert(r.status=="403 Forbidden"&&r.response.find("Press S2 to unlock")!=std::string::npos);
    grant.activate(static_cast<std::uint64_t>(fake_time_us/1000));r=request("/access-check");get(&r);assert(r.status=="204 No Content");
    grant.until=0;r=request("/access-check");get(&r);assert(r.status=="403 Forbidden");
    policy=2;connected=true;r=request("/api/v1/status");get(&r);root=cJSON_Parse(r.response.c_str());assert(cJSON_GetObjectItemCaseSensitive(root,"wifi_rssi_dbm")->valueint==-62);assert(cJSON_GetObjectItemCaseSensitive(root,"gauge_limit_w")->valueint==5000);cJSON_Delete(root);
    fake_ap_info_ok=false;r=request("/api/v1/status");get(&r);root=cJSON_Parse(r.response.c_str());assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(root,"wifi_rssi_dbm")));cJSON_Delete(root);fake_ap_info_ok=true;connected=false;
    // Uptime/status does not require allocating the large reading snapshot.
    reject_allocation_size=sizeof(Snapshot);
    policy=2;r=request("/api/v1/status");get(&r);assert(r.status=="200 OK");
    root=cJSON_Parse(r.response.c_str());assert(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(root,"uptime_ms")));cJSON_Delete(root);
    r=request("/api/v1/values");get(&r);assert(r.status=="503 Service Unavailable");
    assert(r.response.find("snapshot_memory")!=std::string::npos);
    reject_allocation_size=0;
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/status");get(&r);assert(r.status==(p?"200 OK":"403 Forbidden"));}
    // Anonymized fixture identifiers reach JSON as strings, never numeric projections.
    std::ifstream capture("test/fixtures/isk-1-llc.txt");
    const std::string text(std::istreambuf_iterator<char>(capture),{});
    openp1::meter::Bytes bytes;assert(openp1::meter::parse_hex(text,bytes));
    openp1::meter::Processor processor;processor.feed(bytes.data(),bytes.size(),false,100);processor.finish(100);
    snapshot.capture(processor,false);
    policy=1;r=request("/api/v1/values");get(&r);assert(r.status=="200 OK");
    root=cJSON_Parse(r.response.c_str());assert(root);
    auto* values=cJSON_GetObjectItemCaseSensitive(root,"values");assert(cJSON_GetArraySize(values)==16);
    auto* serial=cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(values,0),"value");
    assert(cJSON_IsString(serial) && std::strcmp(serial->valuestring,"12345678")==0);
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(values,0),"name")->valuestring,"Serial number")==0);
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(values,1),"value")->valuestring,"1234567")==0);
    for(int i=0;i<16;++i) {
        auto* item=cJSON_GetArrayItem(values,i);auto* code=cJSON_GetObjectItemCaseSensitive(item,"obis");
        if(std::strcmp(code->valuestring,"1.0.1.8.1.255")==0) {
            assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(item,"name")->valuestring,"Import +A tariff 1")==0);
            assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(item,"obis_display")->valuestring,"1-0:1.8.1")==0);
        }
    }
    cJSON_Delete(root);
    setup=true;trying=false;pending=false;scan.state=ScanState::Idle;
    r=request("/setup/wifi");r.body="{\"ssid\":\"home\",\"password\":\"fake-password\"}";r.content_len=r.body.size();
    assert(post(&r)==ESP_OK && r.status=="202 Accepted" && pending);
    assert(setup_state==SetupState::Connecting);
    assert(r.response.find("fake-password")==std::string::npos);
    pending=false;scan.state=ScanState::Running;r.offset=0;post(&r);assert(r.status=="409 Conflict" && !pending);
    scan.state=ScanState::Idle;r.offset=0;r.headers["Origin"]="http://evil.example";post(&r);assert(r.status=="403 Forbidden");
    r.headers["Origin"]="http://192.168.4.1";r.headers["X-Setup-Token"]="wrong";r.offset=0;post(&r);assert(r.status=="403 Forbidden");
    // Full settings denied to View/None, including during provisioning.
    setup=false;grant.until=0;
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/mqtt");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);
        assert(r.status==(p==2?"200 OK":"403 Forbidden"));}
    for(std::uint8_t p=0;p<3;++p){
        policy=p;r=request("/api/v1/display");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);
        assert(r.status==(p==2?"200 OK":"403 Forbidden"));
    }
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/serial");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);assert(r.status==(p==2?"200 OK":"403 Forbidden"));}
    // Station Wi-Fi scan/configuration: Full only, no credential readback.
    scan={};
    for(std::uint8_t p=0;p<3;++p){
        policy=p;r=request("/api/v1/wifi/scan");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);
        assert(r.status==(p==2?"202 Accepted":"403 Forbidden"));
    }
    policy=2;scan.state=ScanState::Ready;scan.count=1;scan.refresh_after=now_ms()+10000;
    std::strcpy(scan.networks[0].ssid,"<untrusted>");
    r=request("/api/v1/wifi/scan");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);
    assert(r.status=="200 OK"&&r.response.find("<untrusted>")!=std::string::npos);
    r=request("/api/v1/wifi/scan");r.headers["Host"]="evil.example";get(&r);assert(r.status=="403 Forbidden");
    setup=true;r=request("/api/v1/wifi/scan");get(&r);assert(r.status=="403 Forbidden");setup=false;scan={};
    auto wifi_request=[&](const std::string& body){
        fake_time_us+=2000000;r=request("/api/v1/wifi");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;
        r.headers["X-CSRF-Token"]="test-token";r.body=body;r.content_len=r.body.size();
    };
    auto wifi_put=[&](const std::string& body){wifi_request(body);put(&r);};
    const std::string wifi_body=R"({"ssid":"new-network","password":"never-return-this"})";
    wifi_put(wifi_body);assert(r.status=="200 OK");
    wifi_config_t saved_wifi{};std::size_t wifi_size=sizeof(saved_wifi);
    assert(nvs_get_blob(1,"wifi",&saved_wifi,&wifi_size)==ESP_OK);
    assert(std::strcmp(reinterpret_cast<char*>(saved_wifi.sta.ssid),"new-network")==0);
    assert(std::strcmp(reinterpret_cast<char*>(saved_wifi.sta.password),"never-return-this")==0);
    assert(r.response.find("new-network")==std::string::npos&&r.response.find("never-return-this")==std::string::npos);
    assert(!openp1::simulation::restart_due(now_ms()+1499)&&openp1::simulation::restart_due(now_ms()+1500));
    const auto previous_wifi=fake_nvs.at("wifi");
    for(const char* bad:{"{}","[]",R"({"ssid":"","password":""})",R"({"ssid":"home"})",
        R"({"ssid":"home","password":"short"})",R"({"ssid":1,"password":""})",
        R"({"ssid":"a","ssid":"b"})",R"({"ssid":"a","password":"","extra":true})",
        R"({"ssid":"a\u0000b","password":""})"}){
        wifi_put(bad);assert(r.status=="400 Bad Request"&&fake_nvs.at("wifi")==previous_wifi);
    }
    wifi_put(R"({"ssid":")"+std::string(33,'s')+R"(","password":""})");assert(r.status=="400 Bad Request");
    wifi_put(R"({"ssid":"home","password":")"+std::string(64,'p')+R"("})");assert(r.status=="400 Bad Request");
    wifi_put(std::string(769,' '));assert(r.status=="400 Bad Request");
    for(std::uint8_t p=0;p<2;++p){policy=p;wifi_put(wifi_body);assert(r.status=="403 Forbidden");}
    policy=2;setup=true;wifi_put(wifi_body);assert(r.status=="403 Forbidden");setup=false;
    wifi_request(wifi_body);r.headers["Origin"]="http://evil.example";put(&r);assert(r.status=="403 Forbidden");
    wifi_request(wifi_body);r.headers["Host"]="evil.example";put(&r);assert(r.status=="403 Forbidden");
    wifi_request(wifi_body);r.headers["X-CSRF-Token"]="bad";put(&r);assert(r.status=="403 Forbidden");
    wifi_request(wifi_body);fake_after_recv=[](){policy=1;};put(&r);fake_after_recv=nullptr;
    assert(r.status=="403 Forbidden"&&fake_nvs.at("wifi")==previous_wifi);policy=2;
    for(const auto state:{ScanState::Queued,ScanState::Running}){
        scan.state=state;wifi_put(wifi_body);assert(r.status=="409 Conflict");
    }
    scan={};fake_commit_ok=false;wifi_put(wifi_body);assert(r.status=="503 Service Unavailable");
    assert(fake_nvs.at("wifi")==previous_wifi);fake_commit_ok=true;
    wifi_put(R"({"ssid":")"+std::string(32,'s')+R"(","password":""})");assert(r.status=="200 OK");
    assert(fake_nvs.at("wifi")[31]=='s'&&fake_nvs.at("wifi")[32]==0);
    r=request("/api/v1/wifi");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);assert(r.status=="404 Not Found");
    auto serial_put=[&](const char* body){fake_time_us+=2000000;r=request("/api/v1/serial");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;r.headers["X-CSRF-Token"]="test-token";r.body=body;r.content_len=r.body.size();put(&r);};
    policy=2;serial_put("{\"baud\":19200,\"parity\":\"Even\",\"stop_bits\":2,\"inverted\":false}");assert(r.status=="200 OK");
    openp1::input::SerialSettings uart_saved;assert(openp1::serial::get(uart_saved)&&uart_saved.baud==19200&&!uart_saved.inverted);assert(openp1::serial::boot_settings().baud==115200);
    assert(!openp1::simulation::restart_due(static_cast<std::uint64_t>(fake_time_us/1000)+1499));assert(openp1::simulation::restart_due(static_cast<std::uint64_t>(fake_time_us/1000)+1500));
    for(const char* bad:{"{}","{\"baud\":0}","{\"baud\":115201}","{\"baud\":9600.5}","{\"baud\":9600,\"baud\":19200}","{\"parity\":\"Unknown\"}","{\"stop_bits\":3}","{\"inverted\":1}","{\"extra\":true}"}){serial_put(bad);assert(r.status=="400 Bad Request");}
    fake_commit_ok=false;serial_put("{\"baud\":38400}");assert(r.status=="503 Service Unavailable");assert(openp1::serial::get(uart_saved)&&uart_saved.baud==19200);fake_commit_ok=true;
    policy=1;serial_put("{\"baud\":38400}");assert(r.status=="403 Forbidden");
    policy=2;setup=true;serial_put("{\"baud\":38400}");assert(r.status=="403 Forbidden");setup=false;
    policy=2;
    auto display_put=[&](const char* body){
        fake_time_us+=2000000;r=request("/api/v1/display");
        r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;
        r.headers["X-CSRF-Token"]="test-token";r.body=body;r.content_len=r.body.size();put(&r);
    };
    display_put("{\"rotate_180\":true}");assert(r.status=="200 OK" && openp1::display::rotated());
    assert(fake_nvs.at("display_180").at(0)==1);
    display_put("{\"rotate_180\":180}");assert(r.status=="400 Bad Request");
    display_put("{\"rotate_180\":false,\"extra\":true}");assert(r.status=="400 Bad Request");
    fake_commit_ok=false;display_put("{\"rotate_180\":false}");
    assert(r.status=="503 Service Unavailable" && openp1::display::rotated());fake_commit_ok=true;
    display_put("{\"rotate_180\":false}");assert(r.status=="200 OK" && !openp1::display::rotated());
    display_put("{\"gauge_limit_w\":15000}");assert(r.status=="200 OK"&&openp1::dashboard::limit_w==15000);
    std::uint32_t persisted_gauge=0;std::size_t persisted_size=sizeof(persisted_gauge);assert(nvs_get_blob(1,"gauge_w",&persisted_gauge,&persisted_size)==ESP_OK&&persisted_gauge==15000);
    for(const char* bad:{"{\"gauge_limit_w\":0}","{\"gauge_limit_w\":1000001}","{\"gauge_limit_w\":100.5}","{\"gauge_limit_w\":\"10000\"}","{\"gauge_limit_w\":10000,\"gauge_limit_w\":20000}"}){display_put(bad);assert(r.status=="400 Bad Request");}
    fake_commit_ok=false;display_put("{\"gauge_limit_w\":20000}");assert(r.status=="503 Service Unavailable"&&openp1::dashboard::limit_w==15000);fake_commit_ok=true;
    display_put("{\"gauge_limit_w\":10000}");assert(r.status=="200 OK");
    policy=1;display_put("{\"rotate_180\":true}");assert(r.status=="403 Forbidden");
    policy=2;setup=true;display_put("{\"rotate_180\":true}");assert(r.status=="403 Forbidden");setup=false;
    policy=2;r=request("/api/v1/debug");get(&r);assert(r.status=="404 Not Found");
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/mqtt/status");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);assert(r.status==(p==2?"200 OK":"403 Forbidden"));}
    root=cJSON_Parse(r.response.c_str());assert(root);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"connected")));
    assert(cJSON_GetObjectItemCaseSensitive(root,"messages_sent")->valueint==42);
    assert(!cJSON_GetObjectItemCaseSensitive(root,"password"));cJSON_Delete(root);
    r=request("/api/v1/mqtt/status");r.headers["Host"]="evil.example";get(&r);assert(r.status=="403 Forbidden");
    fake_time_us+=2000000;r=request("/api/v1/debug");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;r.headers["X-CSRF-Token"]="test-token";r.body="{}";r.content_len=2;put(&r);assert(r.status=="404 Not Found");
    policy=2;reject_allocation_size=sizeof(Snapshot);
    r=request("/api/v1/values");assert(get(&r)==ESP_OK&&r.status=="503 Service Unavailable");
    reject_allocation_size=sizeof(openp1::mqtt::Config);
    r=request("/api/v1/mqtt");r.headers["Host"]=ip;r.headers.erase("Origin");
    assert(get(&r)==ESP_OK&&r.status=="503 Service Unavailable");reject_allocation_size=0;
    reject_allocation_size=20;
    fake_time_us+=2000000;r=request("/api/v1/access");r.headers["Host"]=ip;
    r.headers["Origin"]=std::string("http://")+ip;r.headers["X-CSRF-Token"]="test-token";
    r.body="{\"policy\":\"Full\"}  ";r.content_len=r.body.size();
    reject_allocation_size=r.content_len+1;
    assert(put(&r)==ESP_OK&&r.status=="503 Service Unavailable");reject_allocation_size=0;
    // Simulation remains disabled on fresh setup and is Full-only.
    for(std::uint8_t p=0;p<3;++p){
        policy=p;r=request("/api/v1/simulation");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);
        assert(r.status==(p==2?"200 OK":"403 Forbidden"));
    }
    auto* simulation_json=cJSON_Parse(r.response.c_str());assert(simulation_json);
    assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(simulation_json,"enabled")));
    assert(std::strcmp(cJSON_GetObjectItemCaseSensitive(simulation_json,"layer")->valuestring,"llc_apdu")==0);
    cJSON_Delete(simulation_json);
    auto simulation_put=[&](const std::string& body){
        fake_time_us+=2000000;r=request("/api/v1/simulation");
        r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;r.headers["X-CSRF-Token"]="test-token";
        r.body=body;r.content_len=r.body.size();put(&r);
    };
    policy=2;
    r=request("/api/v1/simulation");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;
    r.body="{\"enabled\":true}";r.content_len=r.body.size();put(&r);assert(r.status=="403 Forbidden");
    simulation_put("{\"enabled\":true}");assert(r.status=="200 OK");
    assert(!openp1::simulation::boot_data().enabled); // Saved change applies only at reboot.
    for(const char* invalid:{"{\"enabled\":1}","{\"enabled\":true,\"enabled\":false}","{\"enabled\":true,\"layer\":\"guess\"}",
        "{\"enabled\":true,\"text\":\"7\"}","{\"enabled\":true,\"text\":\"zz\"}","{\"enabled\":true,\"text\":\"\"}","{\"enabled\":true,\"unknown\":0}"}){
        simulation_put(invalid);assert(r.status=="400 Bad Request");
    }
    simulation_put(std::string("{\"enabled\":true,\"text\":\"")+std::string(4097,'a')+"\"}");assert(r.status=="400 Bad Request");
    fake_commit_ok=false;simulation_put("{\"enabled\":false}");assert(r.status=="503 Service Unavailable");fake_commit_ok=true;
    openp1::input::SimulationConfig saved_simulation;assert(openp1::simulation::get(saved_simulation)&&saved_simulation.enabled==1);
    policy=1;simulation_put("{\"enabled\":false}");assert(r.status=="403 Forbidden");
    policy=2;setup=true;simulation_put("{\"enabled\":false}");assert(r.status=="403 Forbidden");setup=false;
    fake_after_recv=[](){policy=1;};simulation_put("{\"enabled\":false}");fake_after_recv=nullptr;assert(r.status=="403 Forbidden");
    policy=2;simulation_put("{\"enabled\":false}");assert(r.status=="200 OK");
    reject_allocation_size=sizeof(openp1::input::SimulationConfig);
    r=request("/api/v1/simulation");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);assert(r.status=="503 Service Unavailable");
    reject_allocation_size=0;

    policy=0;grant.activate(1000);fake_time_us=1000000;
    r=request("/api/v1/events");assert(get(&r)==ESP_OK);auto* stream=last_async_request;
    stream_tick();assert(stream->response.find("event: state")!=std::string::npos);
    const auto previous=stream->response.size();fake_time_us=301000000;stream_tick();
    assert(completed_streams==1 && stream->response.size()==previous);delete stream;
    r=request("/api/v1/values");get(&r);assert(r.status=="403 Forbidden");
    policy=1;grant.activate(302000);fake_time_us=302000000;
    r=request("/api/v1/events");get(&r);stream=last_async_request;
    fake_time_us=602000000;stream_tick();assert(completed_streams==1); // View restored: read stream stays authorized.
    policy=0;stream_tick();assert(completed_streams==2);delete stream;
    policy=2;grant.until=0;fake_time_us=700000000;
    r=request("/api/v1/access");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;r.headers["X-CSRF-Token"]="test-token";
    r.body="{\"policy\":\"None\"}";r.content_len=r.body.size();
    policy_save_ok=false;put(&r);assert(r.status=="503 Service Unavailable"&&policy==2);assert(openp1::simulation::restart_due(700000));
    policy_save_ok=true;r.offset=0;put(&r);assert(r.status=="200 OK" && policy==0);
    assert(!openp1::simulation::restart_due(701499)&&openp1::simulation::restart_due(701500));
    r=request("/api/v1/access");get(&r);assert(r.status=="403 Forbidden");

    policy=0;grant.activate(800000);fake_time_us=1099500000;
    r=request("/api/v1/access");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;r.headers["X-CSRF-Token"]="test-token";
    r.body="{\"policy\":\"Full\"}";r.content_len=r.body.size();
    fake_after_recv=[](){fake_time_us=1100000000;};put(&r);fake_after_recv=nullptr;
    assert(r.status=="403 Forbidden" && policy==0); // expiry while receiving the mutation body

    grant.until=0;raw_history.count=1;raw_history.chunks[0].size=3;
    raw_history.chunks[0].bytes[0]=0x7e;raw_history.chunks[0].bytes[1]=0;raw_history.chunks[0].bytes[2]=0xff;
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/raw");get(&r);assert(r.status==(p?"200 OK":"403 Forbidden"));
        if(p)assert(r.response.find("7E00FF")!=std::string::npos);}

    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/diagnostics");get(&r);assert(r.status==(p?"200 OK":"403 Forbidden"));if(p)assert(r.response.find("Raw bytes")!=std::string::npos&&r.response.find("Decoded values")!=std::string::npos);else assert(r.response.find("Press S2 to unlock")!=std::string::npos);}
    setup=true;policy=2;r=request("/diagnostics");get(&r);assert(r.response.find("Decoded values")==std::string::npos);setup=false;
    policy=1;reject_allocation_at_least=1;r=request("/api/v1/raw");get(&r);assert(r.status=="503 Service Unavailable");reject_allocation_at_least=0;
    policy=1;grant.until=0;r=request("/settings");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);
    assert(r.status=="403 Forbidden" && r.response.find("Press S2 to unlock")!=std::string::npos);
    policy=2;r=request("/settings");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);assert(r.status=="200 OK");
    r=request("/settings");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;
    throw_next_new=true;assert(get(&r)==ESP_OK&&!throw_next_new&&r.status=="503 Service Unavailable");
    r=request("/settings");r.headers["Host"]="wrong.example";r.headers.erase("Origin");get(&r);
    assert(r.status=="403 Forbidden" && r.response.find("station_origin_required")!=std::string::npos);

    // Firmware update requires Full + same-origin + CSRF and never writes the running partition.
    grant.until=0;setup=false;
    for(std::uint8_t p=0;p<3;++p){policy=p;r=request("/api/v1/firmware");r.headers["Host"]=ip;r.headers.erase("Origin");get(&r);assert(r.status==(p==2?"200 OK":"403 Forbidden"));}
    std::string image(4096,'x');esp_image_header_t image_header;esp_image_segment_header_t segment;segment.data_len=4064;esp_app_desc_t app;
    std::memcpy(image.data(),&image_header,sizeof(image_header));std::memcpy(image.data()+24,&segment,sizeof(segment));std::memcpy(image.data()+32,&app,sizeof(app));
    auto upload=[&](const std::string& bytes,std::size_t declared=0){fake_time_us+=2000000;r=request("/api/v1/firmware");r.headers["Host"]=ip;r.headers["Origin"]=std::string("http://")+ip;r.headers["X-CSRF-Token"]="test-token";r.headers["Content-Type"]="application/octet-stream";r.body=bytes;r.content_len=declared?declared:bytes.size();put(&r);};
    for(std::uint8_t p=0;p<2;++p){policy=p;upload(image);assert(r.status=="403 Forbidden"&&fake_ota_begins==0);}
    policy=2;fake_ota_layout=false;upload(image);assert(r.status=="409 Conflict"&&fake_ota_begins==0);fake_ota_layout=true;
    fake_ota_same_partition=true;upload(image);assert(r.status=="409 Conflict"&&fake_ota_begins==0);fake_ota_same_partition=false;
    fake_ota_state=ESP_OTA_IMG_PENDING_VERIFY;upload(image);assert(r.status=="409 Conflict"&&fake_ota_begins==0);fake_ota_state=ESP_OTA_IMG_VALID;
    upload(image,openp1::firmware::maximum_image+1);assert(r.status=="413 Payload Too Large"&&fake_ota_begins==0);
    auto invalid=image;invalid[0]=0;upload(invalid);assert(r.status=="400 Bad Request"&&fake_ota_begins==0);
    invalid=image;invalid[12]=9;upload(invalid);assert(r.status=="400 Bad Request"&&fake_ota_begins==0);
    invalid=image;invalid[23]=0;upload(invalid);assert(r.status=="400 Bad Request"&&fake_ota_begins==0);
    invalid=image;invalid[80]='x';upload(invalid);assert(r.status=="400 Bad Request"&&fake_ota_begins==0);
    invalid=image;std::memcpy(invalid.data()+48,"2026.02.30",10);upload(invalid);assert(r.status=="400 Bad Request"&&fake_ota_begins==0);
    // Invalid tokens/origins/content types cannot enter the updater.
    upload(image);assert(r.status=="200 OK"&&fake_ota_selects==1);const auto selected=fake_ota_selects;
    r.offset=0;r.headers["X-CSRF-Token"]="wrong";put(&r);assert(r.status=="403 Forbidden"&&fake_ota_selects==selected);
    r.headers["X-CSRF-Token"]="test-token";r.headers["Origin"]="http://evil.example";put(&r);assert(r.status=="403 Forbidden");
    r.headers["Origin"]=std::string("http://")+ip;r.headers["Content-Type"]="application/json";put(&r);assert(r.status=="403 Forbidden");
    fake_boot=fake_running; // Independent update attempt after a simulated reboot.
    fake_recv_limit=17;upload(image);assert(r.status=="200 OK"&&fake_ota_bytes==std::vector<std::uint8_t>(image.begin(),image.end()));fake_recv_limit=static_cast<std::size_t>(-1);
    fake_boot=fake_running;
    const auto accepted=fake_ota_selects;
    fake_ota_end_ok=false;upload(image);assert(r.status=="400 Bad Request"&&fake_ota_selects==accepted);fake_ota_end_ok=true;
    auto aborted=fake_ota_aborts;upload(image,image.size()+1);assert(r.status=="408 Request Timeout"&&fake_ota_aborts==aborted+1&&fake_ota_selects==accepted);
    fake_ota_write_ok=false;aborted=fake_ota_aborts;upload(image);assert(r.status=="500 Internal Server Error"&&fake_ota_aborts==aborted+1&&fake_ota_selects==accepted);fake_ota_write_ok=true;
    static unsigned ota_reads=0;policy=0;grant.activate(static_cast<std::uint64_t>(fake_time_us/1000));fake_after_recv=[](){if(++ota_reads==2)grant.until=0;};aborted=fake_ota_aborts;upload(image);fake_after_recv=nullptr;assert(r.status=="403 Forbidden"&&fake_ota_aborts==aborted+1&&fake_ota_selects==accepted);
    policy=2;fake_after_ota_end=[](){policy=0;};upload(image);fake_after_ota_end=nullptr;assert(r.status=="403 Forbidden"&&fake_ota_selects==accepted);
    policy=2;fake_after_recv=[](){fake_time_us+=181000000;};upload(image);fake_after_recv=nullptr;assert(r.status=="408 Request Timeout"&&fake_ota_selects==accepted);
    fake_ota_select_ok=false;upload(image);assert(r.status=="500 Internal Server Error"&&fake_ota_selects==accepted);fake_ota_select_ok=true;
    upload(image);assert(r.status=="200 OK"&&fake_ota_selects==accepted+1);assert(!openp1::simulation::restart_due(static_cast<std::uint64_t>(fake_time_us/1000)+1499));
    upload(image);assert(r.status=="409 Conflict"&&fake_ota_selects==accepted+1);
    assert(openp1::firmware::date_version("2028.02.29",11)&&!openp1::firmware::date_version("2026.02.29",11));
    fake_running=&fake_ota0;fake_ota_state=ESP_OTA_IMG_PENDING_VERIFY;openp1::firmware::startup_tick(29999,true);assert(fake_ota_confirms==0);openp1::firmware::startup_tick(30000,true);assert(fake_ota_confirms==1);

}
