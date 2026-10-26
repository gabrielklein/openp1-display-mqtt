#include "simulation/service.hpp"
#include "network.hpp"
#include "display/dashboard.hpp"
#include "model.hpp"
#include "internal.hpp"
#include "esp_event.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "http/stream.hpp"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <cstdio>
#include <cstring>

namespace openp1::network {
namespace detail {
Snapshot snapshot;
RawHistory raw_history;
SetupState setup_state=SetupState::Idle;
Scan scan;
std::uint64_t close_ap_at=0;
SemaphoreHandle_t lock = nullptr;
bool setup = false, connected = false, pending = false, trying = false;
bool ready = false;
unsigned failures = 0;
std::int16_t wifi_signal=-127;
std::uint64_t next_attempt = 0, deadline = 0;
wifi_config_t candidate{};
access::Grant grant;
unsigned reset_countdown=0;
std::uint8_t policy = 0; // Fail closed on missing/corrupt policy; new installation set below.
char ip[16] = "0.0.0.0", ap_name[14]{}, csrf[33]{};
void event(void*, esp_event_base_t base, std::int32_t id, void* data) {
    Guard guard;
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        if (scan.state==ScanState::Running) {
            scan.finished=true;
            scan.successful=static_cast<wifi_event_sta_scan_done_t*>(data)->status==0;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* e = static_cast<ip_event_got_ip_t*>(data);
        std::snprintf(ip,sizeof(ip),IPSTR,IP2STR(&e->ip_info.ip));

        connected = true; failures = 0;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {

        connected = false; std::strcpy(ip,"0.0.0.0");
        next_attempt = now_ms() + retry_delay(failures);
    }
}
void worker(void*) {
#ifdef CONFIG_OPENP1_S2
    access::Button button;

    gpio_config_t button_config{};button_config.pin_bit_mask=1ULL<<35;
    button_config.mode=GPIO_MODE_INPUT;button_config.pull_up_en=GPIO_PULLUP_DISABLE;
    button_config.pull_down_en=GPIO_PULLDOWN_DISABLE;
    const bool button_ready=gpio_config(&button_config)==ESP_OK;
    std::uint64_t reset_at=0;
#endif
    int dns=-1;
    if (setup) {
        dns=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        sockaddr_in local{}; local.sin_family=AF_INET; local.sin_port=htons(53);
        if (dns>=0 && bind(dns,reinterpret_cast<sockaddr*>(&local),sizeof(local))<0) { close(dns); dns=-1; }
        if (dns>=0) {
            timeval timeout{0,10000};
            if (setsockopt(dns,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))<0) { close(dns); dns=-1; }
        }
    }
    std::uint64_t next_signal=0;
    for (;;) {
        const auto now=now_ms();
        if(now>=next_signal){next_signal=now+2000;wifi_ap_record_t info{};const auto status=esp_wifi_sta_get_ap_info(&info);Guard guard;wifi_signal=connected&&status==ESP_OK?info.rssi:-127;}
        if(simulation::restart_due(now))esp_restart();
#ifdef CONFIG_OPENP1_S2
        if(button_ready){
            const auto action=button.update(gpio_get_level(GPIO_NUM_35)==0,now);
            {Guard guard;reset_countdown=button.countdown(now);
                if(action==access::Action::Grant){grant.activate(now);}}
            if(action==access::Action::ResetWifi){

                nvs_handle_t n;auto result=nvs_open("openp1",NVS_READWRITE,&n);
                if(result==ESP_OK){result=nvs_erase_key(n,"wifi");
                    if(result==ESP_ERR_NVS_NOT_FOUND)result=ESP_OK;
                    if(result==ESP_OK)result=nvs_commit(n);
                    nvs_close(n);}
                if(result==ESP_OK){reset_at=now+1000;}
                else ESP_LOGE("access","Wi-Fi reset could not be persisted");
            }
            if(reset_at && now>=reset_at)esp_restart();
        }
#endif
        bool start_scan=false, collect_scan=false, stop_scan=false;
        { Guard guard;
            if (scan.state==ScanState::Queued) {
                scan.state=ScanState::Running; scan.finished=false;
                scan.deadline=now+10000; start_scan=true;
            } else if (scan.state==ScanState::Running) {
                if (scan.finished) collect_scan=true;
                else if (now>=scan.deadline) { scan.state=ScanState::Failed; stop_scan=true; }
            }
        }
        if (start_scan) {
            wifi_scan_config_t config{};
            const auto status=esp_wifi_scan_start(&config,false);
            if (status!=ESP_OK) {
                ESP_LOGW("setup","Wi-Fi scan failed: %s",esp_err_to_name(status));
                Guard guard; scan.state=ScanState::Failed;
            }
        }
        if (stop_scan) { esp_wifi_scan_stop(); esp_wifi_clear_ap_list(); }
        if (collect_scan) {
            wifi_ap_record_t records[16]{}; std::uint16_t count=16;
            const auto status=esp_wifi_scan_get_ap_records(&count,records);
            esp_wifi_clear_ap_list();
            Guard guard;
            scan.count=0;
            if (status==ESP_OK && scan.successful) {
                for (std::size_t i=0;i<count;++i) {
                    auto& item=scan.networks[scan.count++];
                    item={}; std::memcpy(item.ssid,records[i].ssid,32);
                    item.rssi=records[i].rssi; item.secure=records[i].authmode!=WIFI_AUTH_OPEN;
                }
                scan.state=ScanState::Ready;
            } else scan.state=ScanState::Failed;
        }
        bool connect_now=false, finish=false, timeout=false, close_ap=false;
        { Guard guard;
            if (pending) {
                pending=false; trying=true; connected=false; setup_state=SetupState::Connecting;
                deadline=now+30000; connect_now=true;
            }
            if (setup && setup_state==SetupState::Success) {
                if (now>=close_ap_at) close_ap=true;
            } else if (setup && trying && connected) { setup_state=SetupState::Saving; finish=true; }
            else if (setup && trying && now>=deadline) { timeout=true; }
            else if (!setup && !connected && now>=next_attempt) {
                next_attempt=now+retry_delay(failures);
                if (failures<6) ++failures;
                connect_now=true;
            }
        }
        if (connect_now) {
            bool configuring; { Guard guard; configuring=setup; }
            const auto configured=configuring ? esp_wifi_set_config(WIFI_IF_STA,&candidate) : ESP_OK;
            const auto result=configured==ESP_OK ? esp_wifi_connect() : configured;
            if (configuring && result!=ESP_OK) {
                esp_wifi_disconnect();
                Guard guard; candidate={}; trying=false; connected=false; setup_state=SetupState::Failed;
            }
        }
        if (timeout) { esp_wifi_disconnect(); Guard guard; candidate={}; trying=false; setup_state=SetupState::Timeout; }
        if (finish) {
            nvs_handle_t store; esp_err_t status=nvs_open("openp1",NVS_READWRITE,&store);
            if (status==ESP_OK) {
                status=nvs_set_blob(store,"wifi",&candidate,sizeof(candidate));
                if (status==ESP_OK) status=nvs_commit(store);
                nvs_close(store);
            }
            if (status==ESP_OK) {
                Guard guard; setup_state=SetupState::Success;
                close_ap_at=now_ms()+10000; candidate={};
            } else {
                esp_wifi_disconnect(); Guard guard; trying=false; candidate={};
                setup_state=SetupState::StorageError;
            }
        }
        if (close_ap) {
            if (esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK) {
                { Guard guard; setup=false; trying=false; }
                if (dns>=0) { close(dns); dns=-1; }
            }
        }
        if (dns>=0) {
            std::uint8_t bytes[512]; sockaddr_in peer{}; socklen_t size=sizeof(peer);
            const int n=recvfrom(dns,bytes,sizeof(bytes),0,reinterpret_cast<sockaddr*>(&peer),&size);
            if (n>0) { const auto length=dns_reply(bytes,static_cast<std::size_t>(n),sizeof(bytes));
                if (length) sendto(dns,bytes,length,0,reinterpret_cast<sockaddr*>(&peer),size); }
        }
        vTaskDelay(pdMS_TO_TICKS(access::poll_ms));
    }
}
}
using namespace detail;
bool start(input::Source source) {
    snapshot.simulation=source==input::Source::Replay;
    lock=xSemaphoreCreateMutex(); if (!lock) return false;
    // Never erase NVS automatically: corruption must not silently erase credentials/policy.
    if (nvs_flash_init()!=ESP_OK) return false;
    nvs_handle_t store;
    wifi_config_t station{}; bool configured=false;
    if (nvs_open("openp1",NVS_READWRITE,&store)!=ESP_OK) return false;
    std::size_t size=sizeof(station);
    const auto wifi_status=nvs_get_blob(store,"wifi",&station,&size);
    if (wifi_status==ESP_OK) {
        if (size!=sizeof(station) || !station.sta.ssid[0]) { nvs_close(store); return false; }
        configured=true;
    } else if (wifi_status!=ESP_ERR_NVS_NOT_FOUND) { nvs_close(store); return false; }
    std::uint8_t rotation=0;
    const auto rotation_status=nvs_get_u8(store,"display_180",&rotation);
    if ((rotation_status!=ESP_OK && rotation_status!=ESP_ERR_NVS_NOT_FOUND) || rotation>1) {
        nvs_close(store); return false;
    }
    display::set_rotated(rotation==1);
    std::uint32_t gauge=dashboard::default_limit_w;std::size_t gauge_size=sizeof(gauge);
    const auto gauge_status=nvs_get_blob(store,"gauge_w",&gauge,&gauge_size);
    dashboard::limit_w=(gauge_status==ESP_OK&&gauge_size==sizeof(gauge)&&dashboard::valid_limit(gauge))?gauge:dashboard::default_limit_w;

    auto status=nvs_get_u8(store,"policy",&policy);
    bool write_policy=false;
    if(status==ESP_ERR_NVS_NOT_FOUND){policy=access::default_policy;status=ESP_OK;write_policy=true;}
    if(status==ESP_OK && !access::valid(policy))status=ESP_ERR_INVALID_STATE;
    std::uint8_t applied=0;
    if(status==ESP_OK){
        const auto read_marker=nvs_get_u8(store,"policy_defaults",&applied);
        if(read_marker==ESP_ERR_NVS_NOT_FOUND){
            const auto migrated=access::migrate_default(policy,false);
            write_policy=write_policy||migrated!=policy;policy=migrated;
            status=nvs_set_u8(store,"policy_defaults",1);
            if(status==ESP_OK && write_policy)status=nvs_set_u8(store,"policy",policy);
            if(status==ESP_OK)status=nvs_commit(store);
        } else if(read_marker!=ESP_OK || applied!=1)status=ESP_ERR_INVALID_STATE;
        else if(write_policy){status=nvs_set_u8(store,"policy",policy);if(status==ESP_OK)status=nvs_commit(store);}
    }
    nvs_close(store);
    if (status!=ESP_OK || policy>2) return false;
    setup=!configured;
    std::uint8_t mac[6]; esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);
    std::snprintf(ap_name,sizeof(ap_name),"OpenP1-%02X%02X%02X",mac[3],mac[4],mac[5]);
    if (esp_netif_init()!=ESP_OK || esp_event_loop_create_default()!=ESP_OK) return false;
    if (!esp_netif_create_default_wifi_sta()) return false;
    auto* ap_netif=esp_netif_create_default_wifi_ap();
    if (!ap_netif) return false;
    if (setup) {
        const auto stopped=esp_netif_dhcps_stop(ap_netif);
        if (stopped!=ESP_OK && stopped!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) return false;
        esp_netif_ip_info_t info{};
        if (esp_netif_get_ip_info(ap_netif,&info)!=ESP_OK) return false;
        esp_netif_dns_info_t dns{}; dns.ip.type=ESP_IPADDR_TYPE_V4; dns.ip.u_addr.ip4=info.ip;
        std::uint8_t offer_dns=1;
        if (esp_netif_set_dns_info(ap_netif,ESP_NETIF_DNS_MAIN,&dns)!=ESP_OK ||
            esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,
                                  &offer_dns,sizeof(offer_dns))!=ESP_OK) return false;
        // Arm DHCP before the interface is up; AP start then advertises this DNS.
        if (esp_netif_dhcps_start(ap_netif)!=ESP_OK) return false;
    }
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init)!=ESP_OK || esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK ||
        esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,nullptr)!=ESP_OK ||
        esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,event,nullptr)!=ESP_OK ||
        esp_wifi_set_mode(setup?WIFI_MODE_APSTA:WIFI_MODE_STA)!=ESP_OK) return false;
    if (setup) {
        wifi_config_t ap{}; std::strcpy(reinterpret_cast<char*>(ap.ap.ssid),ap_name);
        ap.ap.ssid_len=std::strlen(ap_name); ap.ap.channel=1; ap.ap.max_connection=2; ap.ap.authmode=WIFI_AUTH_OPEN;
        if (esp_wifi_set_config(WIFI_IF_AP,&ap)!=ESP_OK) return false;
    } else if (esp_wifi_set_config(WIFI_IF_STA,&station)!=ESP_OK) return false;
    std::memset(&station,0,sizeof(station));
    if (esp_wifi_start()!=ESP_OK) return false;
    for (unsigned i=0;i<4;++i) std::snprintf(csrf+i*8,9,"%08lx",static_cast<unsigned long>(esp_random()));
    httpd_config_t config=HTTPD_DEFAULT_CONFIG(); config.uri_match_fn=httpd_uri_match_wildcard;
    config.stack_size=12288; config.max_open_sockets=3; config.lru_purge_enable=true;
    config.recv_wait_timeout=3; config.send_wait_timeout=3;
    httpd_handle_t server=nullptr;
    if (httpd_start(&server,&config)!=ESP_OK) return false;
    httpd_uri_t route{}; route.uri="/*"; route.method=HTTP_GET; route.handler=get;
    if (httpd_register_uri_handler(server,&route)!=ESP_OK) { httpd_stop(server); return false; }
    route.method=HTTP_POST; route.handler=post;
    if (httpd_register_uri_handler(server,&route)!=ESP_OK) { httpd_stop(server); return false; }
    route.method=HTTP_PUT;route.handler=put;
    if(httpd_register_uri_handler(server,&route)!=ESP_OK){httpd_stop(server);return false;}
    if(!stream_start()){httpd_stop(server);return false;}
    if (xTaskCreate(worker,"openp1-network",6144,nullptr,3,nullptr)!=pdPASS) { httpd_stop(server); return false; }
    ready=true; return true;
}
void publish(const input::Pipeline& pipeline, std::uint64_t) {
    if (!ready || xSemaphoreTake(lock,0)!=pdTRUE) return;
    snapshot.capture(pipeline.processor(),pipeline.source()==input::Source::Replay);
    raw_history.count=pipeline.size()<4?pipeline.size():4;raw_history.overwritten=pipeline.overwritten();
    const auto first=pipeline.size()-raw_history.count;
    for(std::size_t i=0;i<raw_history.count;++i){raw_history.chunks[i]=pipeline.at(first+i);
        raw_history.sequences[i]=pipeline.overwritten()+first+i;}
    xSemaphoreGive(lock);
}
bool copy_snapshot(meter::PresentationSnapshot& out,bool& station,bool& provisioning){
    using namespace detail;
    if(!ready)return false;
    Guard guard;out=snapshot;station=connected;provisioning=setup;return true;
}
bool detail::save_policy(std::uint8_t value){
    if(!access::valid(value))return false;
    nvs_handle_t n;if(nvs_open("openp1",NVS_READWRITE,&n)!=ESP_OK)return false;
    auto result=nvs_set_u8(n,"policy",value);
    if(result==ESP_OK)result=nvs_set_u8(n,"policy_defaults",1);
    if(result==ESP_OK)result=nvs_commit(n);
                    nvs_close(n);
    if(result!=ESP_OK)return false;
    Guard guard;policy=value;return true;
}
void decorate(display::View& view, std::uint64_t now) {
    // Only the acquisition owner calls this; preserve status on mutex contention.
    static std::array<std::array<char,21>,2> footer{};
    static bool last_connected=false;static std::uint8_t last_bars=0;
    view.lines[6]=footer[0];view.lines[7]=footer[1];view.wifi_connected=last_connected;view.wifi_bars=last_bars;
    if (!ready) { std::snprintf(view.lines[7].data(),21,"WiFi init failed"); return; }
    if (xSemaphoreTake(lock,0)!=pdTRUE) return;
    view.wifi_connected=connected&&!setup;view.wifi_bars=wifi_signal==-127?0:wifi_signal>=-55?3:wifi_signal>=-67?2:1;
    // Fixed footer; never cover power/phase values or expose STA credentials.
    {
        if (setup) {
            std::snprintf(view.lines[6].data(),21,"%s",ap_name);
            std::snprintf(view.lines[7].data(),21,"Setup 192.168.4.1");
        } else {
            std::snprintf(view.lines[6].data(),21,"%s",connected?"WiFi connected":"WiFi reconnecting");
            std::snprintf(view.lines[7].data(),21,"%s",connected?ip:"Retry <= 60s");
        }
    }
    if(reset_countdown){
        std::snprintf(view.lines[6].data(),21,"Hold: reset WiFi");
        std::snprintf(view.lines[7].data(),21,"In %u seconds",reset_countdown);
    } else if(grant.remaining(now) && now/5000%2==1){
        std::snprintf(view.lines[6].data(),21,"Temp full access");
        std::snprintf(view.lines[7].data(),21,"%lu seconds",static_cast<unsigned long>(grant.remaining(now)));
    }
    footer[0]=view.lines[6];footer[1]=view.lines[7];last_connected=view.wifi_connected;last_bars=view.wifi_bars;
    xSemaphoreGive(lock);
}
}
