#pragma once
#include "config.hpp"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"
namespace openp1::mqtt {
// active Config and topic strings must outlive the client. Never accept URI credentials.
inline esp_mqtt_client_config_t transport_config(const Config& c,const char* id,const char* availability) {
    esp_mqtt_client_config_t out{};
    out.broker.address.hostname=c.host;out.broker.address.port=c.port;
    out.broker.address.transport=c.tls?MQTT_TRANSPORT_OVER_SSL:MQTT_TRANSPORT_OVER_TCP;
    if(c.tls){
        out.broker.verification.skip_cert_common_name_check=false;
        if(*c.ca)out.broker.verification.certificate=c.ca;
        else out.broker.verification.crt_bundle_attach=esp_crt_bundle_attach;
    }
    out.credentials.client_id=id;out.credentials.username=*c.username?c.username:nullptr;
    out.credentials.authentication.password=*c.password?c.password:nullptr;
    out.session.last_will.topic=availability;out.session.last_will.msg="offline";
    out.session.last_will.qos=1;out.session.last_will.retain=1;out.session.keepalive=30;
    out.network.disable_auto_reconnect=false;out.network.timeout_ms=3000;out.network.reconnect_timeout_ms=10000;
    out.task.stack_size=6144;out.task.priority=2;out.buffer.size=2048;out.buffer.out_size=2048;
    out.outbox.limit=8192; // Bound unacknowledged QoS traffic on low-RAM ESP32.
    return out;
}
}
