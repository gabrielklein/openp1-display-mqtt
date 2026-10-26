#include "mqtt/json.hpp"
#include "mqtt/payload.hpp"
#include "mqtt/transport.hpp"
#include <cassert>
#include <string>
int main(){
    using namespace openp1::mqtt;
    Config corrupt;corrupt.tls=255;assert(!valid(corrupt));
    Config c;c.text_raw_enabled=1;assert(valid(c));assert(!c.tls && c.port==1883);
    const auto plain=transport_config(c,"device","root/availability");
    assert(plain.broker.address.transport==MQTT_TRANSPORT_OVER_TCP && plain.broker.address.port==1883);
    c.tls=true;c.enabled=true;assert(!valid(c));std::strcpy(c.host,"broker.example");assert(valid(c));
    const auto t=transport_config(c,"device","root/availability");
    assert(t.broker.address.transport==MQTT_TRANSPORT_OVER_SSL);
    assert(!t.broker.verification.skip_cert_common_name_check && t.broker.verification.crt_bundle_attach);
    assert(!t.network.disable_auto_reconnect && t.network.reconnect_timeout_ms==10000);
    assert(t.outbox.limit==8192 && t.session.last_will.retain && t.session.last_will.qos==1);
    assert(std::string(t.session.last_will.msg)=="offline");
    std::strcpy(c.ca,"-----BEGIN CERTIFICATE-----\nfixture\n-----END CERTIFICATE-----");
    const auto private_tls=transport_config(c,"id","root/availability");assert(private_tls.broker.verification.certificate==c.ca);
    assert(!private_tls.broker.verification.crt_bundle_attach && !private_tls.broker.verification.skip_cert_common_name_check);
    std::strcpy(c.username,"private-username");std::strcpy(c.password,"private-password");
    auto* j=config_json(c);char* s=cJSON_PrintUnformatted(j);const std::string json=s;cJSON_free(s);cJSON_Delete(j);
    assert(json.find("private-username")==std::string::npos && json.find("private-password")==std::string::npos);
    assert(json.find("fixture")==std::string::npos);
    assert(json.find("\"raw\"")==std::string::npos);
    j=cJSON_Parse("{\"raw\":true}");assert(!patch_config(j,c));cJSON_Delete(j);
    j=cJSON_Parse("{\"host\":\"new.example\"}");assert(patch_config(j,c));cJSON_Delete(j);
    assert(std::string(c.password)=="private-password");
    for(const char* bad:{"{\"tls\":\"false\"}","{\"port\":0}","{\"port\":1.5}","{\"host\":\"mqtt://u:p@host\"}","{\"root\":\"a/#\"}","{\"enabled\":true,\"enabled\":false}"}){
        Config check=c;j=cJSON_Parse(bad);assert(!patch_config(j,check));cJSON_Delete(j);
    }
    j=cJSON_Parse("{\"root\":\"/sensors/power/p1meter\"}");
    assert(patch_config(j,c));cJSON_Delete(j);
    assert(std::string(c.root)=="/sensors/power/p1meter");
    j=config_json(c);s=cJSON_PrintUnformatted(j);
    assert(std::string(s).find("/sensors/power/p1meter")!=std::string::npos);
    cJSON_free(s);cJSON_Delete(j);
    for(const char* root:{"/","/sensors/power/p1meter/","/sensors/+/p1meter","/sensors/#"}) {
        Config check=c;std::strcpy(check.root,root);assert(!valid(check));
    }
    auto* patch=cJSON_Parse("{\"text_enabled\":false,\"text_id\":\"Custom label\"}");assert(patch_config(patch,c));cJSON_Delete(patch);
    assert(!c.text_enabled&&std::string(c.text_id)=="Custom label");
    patch=cJSON_Parse("{\"text_enabled\":1}");assert(!patch_config(patch,c));cJSON_Delete(patch);
    patch=cJSON_Parse("{\"text_enabled\":false,\"text_raw_enabled\":true}");assert(patch_config(patch,c));cJSON_Delete(patch);assert(!c.text_enabled&&c.text_raw_enabled);
    patch=cJSON_Parse("{\"text_enabled\":true,\"text_raw_enabled\":false}");assert(patch_config(patch,c));cJSON_Delete(patch);assert(c.text_enabled&&!c.text_raw_enabled);
    patch=cJSON_Parse("{\"text_raw_enabled\":1}");assert(!patch_config(patch,c));cJSON_Delete(patch);
    assert(!Config{}.obis_enabled);
    patch=cJSON_Parse("{\"obis_enabled\":true}");assert(patch_config(patch,c));cJSON_Delete(patch);assert(c.obis_enabled);
    patch=cJSON_Parse("{\"obis_enabled\":1}");assert(!patch_config(patch,c));cJSON_Delete(patch);
    assert(Config{}.state_enabled);
    patch=cJSON_Parse("{\"state_enabled\":false}");assert(patch_config(patch,c));cJSON_Delete(patch);assert(!c.state_enabled);
    patch=cJSON_Parse("{\"state_enabled\":true}");assert(patch_config(patch,c));cJSON_Delete(patch);assert(c.state_enabled);
    patch=cJSON_Parse("{\"state_enabled\":1}");assert(!patch_config(patch,c));cJSON_Delete(patch);
    assert(!may_publish(false,true,false));
    assert(may_publish(true,true,false)); // Access=None is deliberately not an input.
    assert(!may_publish(true,true,true));assert(!may_publish(true,false,false));
    openp1::meter::PresentationSnapshot snap;snap.count=1;auto& r=snap.values[0];
    r.numeric=true;r.has_scaler_unit=true;r.unit=30;r.t_ms=100;r.obis={1,0,1,8,7,255};r.scaled=42;
    j=state_json(snap,15100);auto* values=cJSON_GetObjectItemCaseSensitive(j,"values");
    auto* entry=cJSON_GetObjectItemCaseSensitive(values,"1_0_1_8_7_255_a2");assert(entry);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"stale")));cJSON_Delete(j);
    j=discovery_json(r,"root","device");
    assert(std::string(cJSON_GetObjectItemCaseSensitive(j,"state_class")->valuestring)=="total_increasing");
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(j,"availability"))==2);cJSON_Delete(j);
}
