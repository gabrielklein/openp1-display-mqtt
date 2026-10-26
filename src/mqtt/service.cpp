#include "support/memory.hpp"
#include "service.hpp"
#include "payload.hpp"
#include "text.hpp"
#include "transport.hpp"
#include "connectivity/network.hpp"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "nvs.h"
#include "mbedtls/x509_crt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <atomic>
#include <ctime>
#include <memory>
#include <new>
namespace openp1::mqtt {
namespace {
SemaphoreHandle_t mutex=nullptr;
Config stored;
std::uint32_t revision=1;
std::atomic<bool> linked{false},service_ready{false};
std::atomic<input::Source> raw_source{input::Source::Uart};
std::atomic<unsigned> generation{0},state{0};
// Frame boundaries come from the shared parser, never from scanning every 0x7e.
struct Wire {std::array<std::uint8_t,2049> bytes{};std::size_t size=0;input::Source source=input::Source::Uart;};
struct Packet {std::array<std::uint8_t,2048> bytes{};std::size_t size=0;input::Source source=input::Source::Uart;bool pending=false;};
QueueHandle_t wire_queue=nullptr;
SemaphoreHandle_t packet_mutex=nullptr;
memory::Pointer<Packet> packet;
std::atomic<bool> text_capture{false};
std::atomic<unsigned> text_lost{0};
const char* states[]={"disabled","waiting_wifi","switching_channel","waiting_time","connecting","connected","disconnected","error"};
std::uint64_t now_ms(){return static_cast<std::uint64_t>(esp_timer_get_time()/1000);}
std::atomic<std::uint32_t> sent{0},publish_failures{0},errors{0};
std::atomic<Failure> last_error{Failure::None};
std::atomic<int> error_type{0},broker_code{0},tls_code{0},tls_stack_code{0},socket_code{0};
void failure(Failure error){if(error!=Failure::Connection){error_type=0;broker_code=0;tls_code=0;tls_stack_code=0;socket_code=0;}++errors;last_error=error;}
void event(void*,esp_event_base_t,std::int32_t id,void* data) {
    if(id==MQTT_EVENT_CONNECTED){linked=true;++generation;state=5;}
    else if(id==MQTT_EVENT_DISCONNECTED || id==MQTT_EVENT_ERROR){
        linked=false;state=id==MQTT_EVENT_ERROR?7:6;
        if(id==MQTT_EVENT_ERROR){
            error_type=broker_code=tls_code=tls_stack_code=socket_code=0;
            const auto* e=static_cast<esp_mqtt_event_t*>(data);
            if(e && e->error_handle){const auto* detail=e->error_handle;
                error_type=static_cast<int>(detail->error_type);broker_code=static_cast<int>(detail->connect_return_code);
                tls_code=detail->esp_tls_last_esp_err;tls_stack_code=detail->esp_tls_stack_err;socket_code=detail->esp_transport_sock_errno;
            }
            failure(Failure::Connection);
        }
    }
}
int publish(esp_mqtt_client_handle_t client,const char* topic,const char* payload,int length,int qos,int retain){
    const int result=esp_mqtt_client_publish(client,topic,payload,length,qos,retain);
    if(result>=0)++sent;else{++publish_failures;failure(Failure::Publish);}
    return result;
}
bool publish_json(esp_mqtt_client_handle_t client,const std::string& topic,cJSON* root,bool retain,int qos,std::size_t maximum) {
    if(!root)return false;
    char* text=cJSON_PrintUnformatted(root);cJSON_Delete(root);if(!text)return false;
    const auto length=std::strlen(text);
    const bool result=length<=maximum && publish(client,topic.c_str(),text,static_cast<int>(length),qos,retain)>=0;
    cJSON_free(text);return result;
}
bool publish_hex(esp_mqtt_client_handle_t client,const std::string& topic,const std::uint8_t* bytes,std::size_t size){
    if(!size||size>2049){++text_lost;return false;}
    auto hex=memory::buffer(size*2+1);if(!hex){++text_lost;return false;}
    constexpr char digits[]="0123456789ABCDEF";
    for(std::size_t i=0;i<size;++i){hex.get()[i*2]=digits[bytes[i]>>4];hex.get()[i*2+1]=digits[bytes[i]&15];}
    hex.get()[size*2]=0;
    if(publish(client,topic.c_str(),hex.get(),static_cast<int>(size*2),0,0)<0){++text_lost;return false;}
    return true;
}
void worker(void*) {
    auto active=memory::make<Config>();
    auto snapshot=memory::make<meter::PresentationSnapshot>();
    while(!active||!snapshot){
        state=7;failure(Failure::Memory);vTaskDelay(pdMS_TO_TICKS(1000));
        if(!active)active=memory::make<Config>();
        if(!snapshot)snapshot=memory::make<meter::PresentationSnapshot>();
    }
    std::uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);char device[13],identity[24],client_id[32];
    std::snprintf(device,sizeof(device),"%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    std::string root,availability,ntp;
    esp_mqtt_client_handle_t client=nullptr;
    std::uint32_t loaded=0;unsigned seen_generation=0;
    struct Advertised {std::array<std::uint8_t,6> obis{};std::uint16_t attribute=0;std::uint8_t type=0,unit=0;bool scaler=false,active=false;};
    std::array<Advertised,64> advertised{};
    std::uint64_t next_state=0,next_connect=0,last_notifications=0,recovery_until=0;
    bool ntp_started=false,availability_sent=false,mode_initialized=false,active_simulation=false;

    auto wire_item=memory::Pointer<Wire>();
    for(;;) {
      try{
        if(now_ms()<recovery_until){vTaskDelay(pdMS_TO_TICKS(1000));continue;}
        bool changed;
        xSemaphoreTake(mutex,portMAX_DELAY);changed=loaded!=revision;xSemaphoreGive(mutex);
        bool online=false,setup=true;
        const bool copied=network::copy_snapshot(*snapshot,online,setup);
        const bool mode_changed=copied&&(!mode_initialized||active_simulation!=snapshot->simulation);
        if(changed || mode_changed || !copied || !may_publish(active->enabled,online,setup)) {
            text_capture=false;
            if(client){
                if(linked)publish(client,availability.c_str(),"offline",0,1,1);
                esp_mqtt_client_stop(client);esp_mqtt_client_destroy(client);client=nullptr;linked=false;
            }
            if(wire_queue)xQueueReset(wire_queue);
            if(packet_mutex&&xSemaphoreTake(packet_mutex,0)==pdTRUE){packet->pending=false;xSemaphoreGive(packet_mutex);}
            if(changed) {
                xSemaphoreTake(mutex,portMAX_DELAY);*active=stored;loaded=revision;xSemaphoreGive(mutex);
                if(ntp!=active->ntp){if(ntp_started){esp_netif_sntp_deinit();ntp_started=false;}ntp=active->ntp;}
            }
            if(changed || mode_changed){
                if(copied){active_simulation=snapshot->simulation;mode_initialized=true;}
                const auto base=*active->root?std::string(active->root):std::string("openp1/")+device;
                root=base+(active_simulation?"/simulation":"");availability=root+"/availability";
                std::snprintf(identity,sizeof(identity),"%s%s",device,active_simulation?"_simulation":"");
                std::snprintf(client_id,sizeof(client_id),"openp1_%s",identity);
                raw_source=active_simulation?input::Source::Replay:input::Source::Uart;
                advertised={};next_connect=0;next_state=0;

            }
            state=!active->enabled?0:mode_changed?2:1;
        }
        const bool allowed=copied && may_publish(active->enabled,online,setup);
        if(allowed && active->tls && std::time(nullptr)<1704067200) {
            state=3;
            if(!ntp_started && !ntp.empty()){
                esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp.c_str());
                ntp_started=esp_netif_sntp_init(&config)==ESP_OK;
            }
        } else if(allowed) {
            if(!client && now_ms()>=next_connect){
                next_connect=now_ms()+5000;
                auto config=transport_config(*active,client_id,availability.c_str());

                client=esp_mqtt_client_init(&config);
                state=4;
                if(!client || esp_mqtt_client_register_event(client,static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),event,nullptr)!=ESP_OK ||
                    esp_mqtt_client_start(client)!=ESP_OK){if(client)esp_mqtt_client_destroy(client);client=nullptr;state=7;failure(Failure::Start);}
            }
            if(client && linked) {
                if(active->text_raw_enabled){
                    if(!wire_queue)wire_queue=xQueueCreate(4,sizeof(Wire));
                    if(!packet)packet=memory::make<Packet>();
                    if(packet&&!packet_mutex)packet_mutex=xSemaphoreCreateMutex();
                    if(!wire_item)wire_item=memory::make<Wire>();
                    text_capture=wire_queue&&packet&&packet_mutex&&wire_item;
                    if(!text_capture)++text_lost;
                }else text_capture=false;
                if(seen_generation!=generation.load()){
                    seen_generation=generation;advertised={};next_state=0;availability_sent=false;
                }
                if(!availability_sent)availability_sent=publish(client,availability.c_str(),"online",0,1,1)>=0;
                if(active->discovery&&active->state_enabled) {
                    unsigned sent=0;
                    for(std::size_t i=0;i<snapshot->count && sent<4;++i){
                        const auto& r=snapshot->values[i];if(!r.numeric || r.attribute!=2)continue;
                        const auto k=key(r);
                        const auto& previous=advertised[i];
                        if(previous.active&&previous.obis==r.obis&&previous.attribute==r.attribute&&previous.type==r.type&&previous.unit==r.unit&&previous.scaler==r.has_scaler_unit)continue;
                        const auto topic=std::string("homeassistant/sensor/")+client_id+'/'+k+"/config";
                        if(publish_json(client,topic,discovery_json(r,root.c_str(),identity,active_simulation),true,1,2048)){advertised[i]={r.obis,r.attribute,r.type,r.unit,r.has_scaler_unit,true};++sent;}
                        else break;
                    }
                }
                const auto now=now_ms();
                if(now>=next_state || last_notifications!=snapshot->counters.notifications){
                    if(active->state_enabled){
                        const auto state_topic=root+"/state";
                        publish_json(client,state_topic,state_json(*snapshot,now),false,0,24576);
                    }
                    if(active->text_enabled){
                        char summary[192];
                        if(snapshot->count&&*active->text_id&&publish(client,(root+"/id").c_str(),active->text_id,0,0,0)<0)++text_lost;
                        for(const auto* name:{"active","voltage","current","metering"}){
                            if(text_summary(*snapshot,now,name,summary,sizeof(summary))&&publish(client,(root+"/"+name).c_str(),summary,0,0,0)<0)++text_lost;
                        }
                    }
                    if(active->obis_enabled){
                        char value[513];
                        for(std::size_t i=0;i<snapshot->count;++i){
                            if(obis_payload(*snapshot,i,now,value,sizeof(value))){
                                const auto topic=root+'/'+meter::obis_display(snapshot->values[i].obis);
                                publish(client,topic.c_str(),value,0,0,0);
                            }
                        }
                    }
                    last_notifications=snapshot->counters.notifications;next_state=now+5000;
                }
                if(text_capture){
                    for(unsigned i=0;i<4&&xQueueReceive(wire_queue,wire_item.get(),0)==pdTRUE;++i){
                        if(wire_item->source!=raw_source.load()){++text_lost;continue;}
                        publish_hex(client,root+"/raw",wire_item->bytes.data(),wire_item->size);
                    }
                    if(xSemaphoreTake(packet_mutex,0)==pdTRUE){
                        struct Release {SemaphoreHandle_t mutex;~Release(){xSemaphoreGive(mutex);}} release{packet_mutex};
                        if(packet->pending){
                            if(packet->source==raw_source.load())publish_hex(client,root+"/hdlc",packet->bytes.data(),packet->size);
                            else ++text_lost;
                            packet->pending=false;
                        }
                    }
                }
            } else {text_capture=false;}
        }
        vTaskDelay(pdMS_TO_TICKS(250));
      }catch(const std::bad_alloc&){
        failure(Failure::Memory);recovery_until=now_ms()+15000;
        text_capture=false;state=7;loaded=0;mode_initialized=false;
        if(client){esp_mqtt_client_stop(client);esp_mqtt_client_destroy(client);client=nullptr;linked=false;}
        advertised={};state=7;
        ESP_LOGW("mqtt","Memory exhausted; retry in 15 seconds; free %u, largest block %u bytes",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));

        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }
}
}
const char* error_name(Failure error){switch(error){case Failure::None:return "None";case Failure::Connection:return "Broker authentication, TLS or transport error";case Failure::Publish:return "Publish rejected by MQTT client";case Failure::Start:return "MQTT client could not start";case Failure::Memory:return "Not enough memory; retry deferred";}return "Unknown";}
Statistics statistics(){return {linked.load(),sent.load(),publish_failures.load(),errors.load(),last_error.load(),error_type.load(),broker_code.load(),tls_code.load(),tls_stack_code.load(),socket_code.load()};}
bool start() {
    mutex=xSemaphoreCreateMutex();
    if(!mutex){state=7;failure(Failure::Memory);return false;}
    nvs_handle_t n;const auto opened=nvs_open("openp1",NVS_READONLY,&n);
    if(opened==ESP_OK){std::size_t size=sizeof(stored);const auto result=nvs_get_blob(n,"mqtt",&stored,&size);nvs_close(n);
        const bool legacy=result==ESP_OK&&size==config_v1_size&&stored.version==1;
        if(legacy){
            stored.version=3;stored.text_enabled=1;stored.text_raw_enabled=1;std::strcpy(stored.text_id,"ISK");
        }
        if(result==ESP_OK&&size==sizeof(stored)&&stored.version==2){stored.version=3;stored.text_raw_enabled=stored.text_enabled;}
        if(result==ESP_OK&&stored.version==3){stored.version=4;stored.obis_enabled=0;}
        if(result==ESP_OK&&stored.version==4){stored.version=5;stored.state_enabled=1;}
        if(result!=ESP_ERR_NVS_NOT_FOUND && (result!=ESP_OK||(size!=sizeof(stored)&&!legacy)||!valid(stored))){stored={};state=7;failure(Failure::Start);return false;}}
    else if(opened!=ESP_ERR_NVS_NOT_FOUND){state=7;failure(Failure::Start);return false;}
    service_ready=xTaskCreate(worker,"openp1-mqtt",8192,nullptr,2,nullptr)==pdPASS;
    if(!service_ready){state=7;failure(Failure::Start);}
    return service_ready;
}
bool get(Config& config){if(!service_ready)return false;xSemaphoreTake(mutex,portMAX_DELAY);config=stored;xSemaphoreGive(mutex);return true;}
bool configure(const Config& config){
    if(!service_ready||!valid(config))return false;
    if(*config.ca){mbedtls_x509_crt cert;mbedtls_x509_crt_init(&cert);
        const int parsed=mbedtls_x509_crt_parse(&cert,reinterpret_cast<const unsigned char*>(config.ca),std::strlen(config.ca)+1);
        mbedtls_x509_crt_free(&cert);if(parsed!=0)return false;}
    nvs_handle_t n;if(nvs_open("openp1",NVS_READWRITE,&n)!=ESP_OK)return false;
    auto result=nvs_set_blob(n,"mqtt",&config,sizeof(config));if(result==ESP_OK)result=nvs_commit(n);nvs_close(n);
    if(result!=ESP_OK)return false;

    xSemaphoreTake(mutex,portMAX_DELAY);stored=config;++revision;text_capture=false;xSemaphoreGive(mutex);return true;
}
const char* status(){return states[state.load()];}
std::uint32_t text_dropped(){return text_lost;}
void wire(const std::uint8_t* body,std::size_t size,input::Source source){
    if(!text_capture||source!=raw_source.load())return;
    if(!body||size<7||size>2047){++text_lost;return;}
    Wire item;item.size=size+2;item.source=source;item.bytes[0]=0x7e;
    std::memcpy(item.bytes.data()+1,body,size);item.bytes[size+1]=0x7e;
    if(xQueueSend(wire_queue,&item,0)!=pdTRUE)++text_lost;
}
void apdu(const meter::Bytes& bytes,input::Source source){
    if(!text_capture||source!=raw_source.load())return;
    if(bytes.empty()||bytes.size()>2048){++text_lost;return;}
    if(xSemaphoreTake(packet_mutex,0)!=pdTRUE){++text_lost;return;}
    if(packet->pending)++text_lost;
    std::memcpy(packet->bytes.data(),bytes.data(),bytes.size());packet->size=bytes.size();packet->source=source;packet->pending=true;
    xSemaphoreGive(packet_mutex);
}
}
