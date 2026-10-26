#pragma once
#include "esp_stub.hpp"
constexpr int MQTT_TRANSPORT_OVER_SSL=1,MQTT_TRANSPORT_OVER_TCP=2;
struct esp_mqtt_client_config_t {
    struct {struct {const char* hostname=nullptr;unsigned port=0;int transport=0;}address;
        struct {bool skip_cert_common_name_check=false;const char* certificate=nullptr;int(*crt_bundle_attach)(void*)=nullptr;}verification;}broker;
    struct {const char* client_id=nullptr;const char* username=nullptr;struct {const char* password=nullptr;}authentication;}credentials;
    struct {struct {const char* topic=nullptr;const char* msg=nullptr;int qos=0,retain=0;}last_will;int keepalive=0;}session;
    struct {int timeout_ms=0,reconnect_timeout_ms=0;bool disable_auto_reconnect=false;}network;
    struct {int priority=0,stack_size=0;}task;
    struct {int size=0,out_size=0;}buffer;
    struct {std::uint64_t limit=0;}outbox;
};
#include <vector>
using esp_mqtt_event_id_t=int;
constexpr int MQTT_EVENT_CONNECTED=1,MQTT_EVENT_DISCONNECTED=2,MQTT_EVENT_ERROR=3,MQTT_EVENT_PUBLISHED=4;
struct FakeMqttError {int error_type=0,connect_return_code=0,esp_tls_last_esp_err=0,esp_tls_stack_err=0,esp_transport_sock_errno=0;};
struct esp_mqtt_event_t {int msg_id=0;FakeMqttError* error_handle=nullptr;};
using FakeCallback=void(*)(void*,esp_event_base_t,std::int32_t,void*);
struct FakeMqtt {esp_mqtt_client_config_t config;FakeCallback callback=nullptr;bool connected=false;};
using esp_mqtt_client_handle_t=FakeMqtt*;
inline std::vector<FakeMqtt*> fake_clients;
struct Published {std::string topic,payload;int qos;bool retain;};
inline std::vector<Published> fake_published;
inline unsigned fake_publish_rejections=0;
inline esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t* c){auto* p=new FakeMqtt{*c,nullptr,false};fake_clients.push_back(p);return p;}
inline int esp_mqtt_client_register_event(FakeMqtt* c,int,FakeCallback cb,void*){c->callback=cb;return ESP_OK;}
inline int esp_mqtt_client_start(FakeMqtt* c){c->connected=true;c->callback(nullptr,nullptr,MQTT_EVENT_CONNECTED,nullptr);return ESP_OK;}
inline int esp_mqtt_client_stop(FakeMqtt* c){c->connected=false;c->callback(nullptr,nullptr,MQTT_EVENT_DISCONNECTED,nullptr);return ESP_OK;}
inline int esp_mqtt_client_destroy(FakeMqtt*){return ESP_OK;}
inline int esp_mqtt_client_publish(FakeMqtt* c,const char* topic,const char* payload,int length,int qos,int retain){if(!c->connected)return -1;if(fake_publish_rejections){--fake_publish_rejections;return -1;}fake_published.push_back({topic,length?std::string(payload,static_cast<std::size_t>(length)):std::string(payload),qos,retain!=0});return static_cast<int>(fake_published.size());}
