#include "mqtt/service.hpp"
#include "cJSON.h"
#include "meter/hex.hpp"
#include "mqtt_client.h"
#include "esp_netif_sntp.h"
#include "nvs.h"
#include "mbedtls/x509_crt.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "meter/presentation.hpp"
#include <cassert>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <iterator>
std::int64_t fake_time_us=1000000;
std::time_t fake_epoch=1800000000;
extern "C" std::time_t __wrap_time(std::time_t* out){if(out)*out=fake_epoch;return fake_epoch;}
namespace {
bool station=true,provisioning=false,simulation=false,inject_oom=false;
openp1::meter::PresentationSnapshot meter_snapshot;
}
namespace openp1::network {
bool copy_snapshot(meter::PresentationSnapshot& out,bool& online,bool& setup){
    if(inject_oom){inject_oom=false;throw std::bad_alloc();}
    out=meter_snapshot;
    online=station;setup=provisioning;out.simulation=simulation;return true;
}
}
int main(int argc,char**){
    using namespace openp1;
    const bool initial_simulation=argc>1;simulation=initial_simulation;
    std::ifstream capture_file("test/fixtures/isk-1-llc.txt");const std::string capture_text(std::istreambuf_iterator<char>(capture_file),{});
    meter::Bytes capture_bytes;assert(meter::parse_hex(capture_text,capture_bytes));
    meter::Processor processor;processor.feed(capture_bytes.data(),capture_bytes.size(),false,100);processor.finish(100);
    meter_snapshot.capture(processor,false);
    // Load a version-1 blob to verify migration preserves the configured prefix/credentials.
    mqtt::Config previous;previous.version=argc>3?4:argc>2?3:initial_simulation?2:1;previous.state_enabled=0;previous.obis_enabled=1;previous.text_raw_enabled=0;std::strcpy(previous.root,"saved/prefix");std::strcpy(previous.username,"old-user");
    assert(nvs_set_blob(1,"mqtt",&previous,initial_simulation?sizeof(previous):mqtt::config_v1_size)==ESP_OK&&nvs_commit(1)==ESP_OK);
    assert(mqtt::start());mqtt::Config migrated;assert(mqtt::get(migrated));
    assert(migrated.version==5&&migrated.state_enabled&&(migrated.obis_enabled==(argc>3?1:0))&&(migrated.text_raw_enabled==(argc>2?0:1))&&migrated.text_enabled&&std::string(migrated.root)=="saved/prefix"&&std::string(migrated.username)=="old-user");
    mqtt::Config c;c.enabled=true;c.obis_enabled=true;c.tls=false;c.discovery=true;std::strcpy(c.host,"broker.example");
    if(initial_simulation)std::strcpy(c.root,"custom/test");
    const std::string prefix=initial_simulation?"custom/test":"openp1/010203040506";
    const auto branch=[&](bool sim){return prefix+(sim?"/simulation":"");};
    std::strcpy(c.username,"secret-user");std::strcpy(c.password,"secret-password");
    assert(mqtt::configure(c));
    auto rejected=c;std::strcpy(rejected.ca,"-----BEGIN CERTIFICATE-----\nx\n-----END CERTIFICATE-----");
    fake_bad_certificate=true;assert(!mqtt::configure(rejected));fake_bad_certificate=false;
    fake_commit_ok=false;assert(!mqtt::configure(rejected));fake_commit_ok=true;
    mqtt::Config check;assert(mqtt::get(check)&&!check.ca[0]);
    auto capture=[&](){
        input::Pipeline pipeline;pipeline.select(simulation?input::Source::Replay:input::Source::Uart);
        pipeline.observe([&](const std::uint8_t* data,std::size_t size,std::uint64_t){mqtt::wire(data,size,pipeline.source());},
            [&](const meter::Bytes& data,std::uint64_t){mqtt::apdu(data,pipeline.source());});
        std::ifstream f("test/fixtures/isk-1-wire.txt");std::string text(std::istreambuf_iterator<char>(f),{});
        input::Replay replay;std::string error;assert(replay.load(text,input::Layer::WireHdlc,error));
        // Replay loader emits only to Replay; feed the original decoded lines for either mode.
        std::size_t pos=0;
        while(pos<text.size()){
            const auto end=text.find('\n',pos),space=text.find(' ',pos);meter::Bytes bytes;
            assert(meter::parse_hex(text.substr(space+1,end-space-1),bytes));
            for(std::size_t i=0;i<bytes.size();i+=7)assert(pipeline.ingest(pipeline.source(),input::Layer::WireHdlc,bytes.data()+i,std::min<std::size_t>(7,bytes.size()-i),100));
            pos=end+1;
        }
    };
    std::size_t before_disconnect=0,before_switch=0,before_setup=0,before_text_off=0;
    fake_tick_hook=[&](unsigned tick){
        if(tick==1){fake_publish_rejections=1;capture();mqtt::apdu(meter::Bytes(2049,0),simulation?input::Source::Replay:input::Source::Uart);}

        if(tick==3){auto* client=fake_clients.back();client->connected=false;client->callback(nullptr,nullptr,MQTT_EVENT_DISCONNECTED,nullptr);before_disconnect=fake_published.size();}
        if(tick==4){FakeMqttError error;error.error_type=1;error.connect_return_code=5;error.esp_transport_sock_errno=111;esp_mqtt_event_t event;event.error_handle=&error;
            fake_clients.back()->callback(nullptr,nullptr,MQTT_EVENT_ERROR,&event);
            const auto stats=mqtt::statistics();assert(!stats.connected&&stats.broker_code==5&&stats.socket_code==111&&stats.publish_failures==1);
        }
        if(tick==5){assert(fake_published.size()==before_disconnect);auto* client=fake_clients.back();client->connected=true;client->callback(nullptr,nullptr,MQTT_EVENT_CONNECTED,nullptr);}
        if(tick==7){
            simulation=!simulation;before_switch=fake_published.size();
        }
        if(tick==8){
            capture();
            assert(fake_clients.size()==2);
            assert(std::string(fake_clients.back()->config.session.last_will.topic)==branch(simulation)+"/availability");

        }
        if(tick==9){
            assert(fake_published.at(before_switch).topic==branch(initial_simulation)+"/availability");
            assert(fake_published.at(before_switch).payload=="offline");
            for(std::size_t i=before_switch+1;i<fake_published.size();++i){
                const auto& p=fake_published[i];
                if(p.topic.find("homeassistant/")!=0)assert(p.topic.find(branch(simulation)+"/")==0);
            }
            simulation=initial_simulation;c.tls=true;c.state_enabled=false;c.text_enabled=false;c.text_raw_enabled=true;fake_epoch=0;assert(mqtt::configure(c));
        }
        if(tick==12){assert(fake_ntp_calls==1);assert(fake_clients.size()==2);fake_epoch=1800000000;}
        if(tick==13){ // Text frames and APDU still publish after TLS reconnect.
            before_text_off=fake_published.size();capture();
        }
        if(tick==15){
            bool got_text=false;
            for(std::size_t i=before_text_off;i<fake_published.size();++i){
                assert(fake_published[i].topic!=branch(simulation)+"/state");
                assert(fake_published[i].topic.find("homeassistant/")!=0);
                assert(fake_published[i].topic!=branch(simulation)+"/id"&&fake_published[i].topic!=branch(simulation)+"/active");
                if(fake_published[i].topic==branch(simulation)+"/raw")got_text=true;
            }
            assert(got_text);assert(fake_clients.size()==3);const auto& tls=fake_clients.back()->config;
            assert(!tls.broker.verification.skip_cert_common_name_check && tls.broker.verification.crt_bundle_attach);
            assert(std::string(tls.session.last_will.topic)==branch(simulation)+"/availability");
            provisioning=true;before_setup=fake_published.size();}
        if(tick==17){
            assert(fake_published.size()==before_setup+1);provisioning=false;
            c.tls=false;c.state_enabled=true;c.obis_enabled=false;c.text_enabled=true;c.text_raw_enabled=false;assert(mqtt::configure(c));before_text_off=fake_published.size();
        }
        if(tick==18){
            capture();
        }
        if(tick==19){
            for(std::size_t i=before_text_off;i<fake_published.size();++i)assert(fake_published[i].topic.find("/1-0:")==std::string::npos);
            bool summary=false;for(std::size_t i=before_text_off;i<fake_published.size();++i)if(fake_published[i].topic==branch(simulation)+"/id")summary=true;assert(summary);
            for(std::size_t i=before_text_off;i<fake_published.size();++i)assert(fake_published[i].topic.find("/raw")==std::string::npos&&fake_published[i].topic.find("/hdlc")==std::string::npos);
            inject_oom=true;
        }
        if(tick==21){assert(!mqtt::statistics().connected);assert(std::string(mqtt::status())=="error");}
        if(tick==22)fake_time_us+=15000000;
        if(tick==23){assert(std::string(mqtt::status())=="connected");throw FakeStop{};}
    };
    try{fake_tasks.at("openp1-mqtt")(nullptr);}catch(const FakeStop&){}
    bool obis=false;for(const auto& p:fake_published)if(p.topic==branch(initial_simulation)+"/1-0:51.7.0"){assert(p.payload=="3");obis=true;}assert(obis);
    const auto stats=mqtt::statistics();
    assert(stats.connected&&stats.sent==fake_published.size());
    assert(stats.errors>=3&&stats.publish_failures==1&&stats.last_error==mqtt::Failure::Memory);
    assert(stats.broker_code==0&&stats.socket_code==0);
    unsigned online_messages=0,discovery=0,normal_states=0,simulation_states=0,text_raw=0,text_apdu=0,text_active=0;
    for(const auto& p:fake_published){
        if(p.topic.find("/availability")!=std::string::npos){assert(p.retain&&p.qos==1);if(p.payload=="online")++online_messages;}
        assert(p.topic.find("/text/")==std::string::npos);
        if(p.topic.find("/state")!=std::string::npos){
            const bool sim=p.topic==branch(true)+"/state";
            assert(p.topic==branch(sim)+"/state");
            assert(p.payload.find(sim?"\"simulation\":true":"\"simulation\":false")!=std::string::npos);
            if(sim)++simulation_states;else ++normal_states;
        }
        if(p.topic.find("homeassistant/")!=0 && p.topic!=branch(false)+"/state" && p.topic!=branch(true)+"/state" && p.topic.find("/availability")==std::string::npos){
            assert(!p.retain&&p.qos==0);
            if(p.topic.find("/id")!=std::string::npos)assert(p.payload=="ISK");
            if(p.topic.find("/active")!=std::string::npos){assert(p.payload=="950 0 9281.47 5876.41");++text_active;}
            if(p.topic.find("/voltage")!=std::string::npos)assert(p.payload=="241.40 241.70 239.90 242.60");
            if(p.topic.find("/current")!=std::string::npos)assert(p.payload=="4.61 0.30 3.00 1.31");
            if(p.topic.find("/metering")!=std::string::npos)assert(p.payload=="1537730 7743741 154430 5721982");
            if(p.topic.find("/raw")!=std::string::npos){assert(p.payload.front()=='7'&&p.payload.substr(p.payload.size()-2)=="7E");++text_raw;}
            if(p.topic.find("/hdlc")!=std::string::npos){assert(p.payload.find("E6E7000F")==0);++text_apdu;}
        }
        if(p.topic.find("homeassistant/")==0){
            assert(p.retain);++discovery;
            const bool sim=p.topic.find("_simulation/")!=std::string::npos;
            auto* json=cJSON_Parse(p.payload.c_str());assert(json);
            assert(std::string(cJSON_GetObjectItemCaseSensitive(json,"state_topic")->valuestring)==branch(sim)+"/state");
            const auto* device=cJSON_GetObjectItemCaseSensitive(json,"device");
            const std::string expected_id=std::string("openp1_010203040506")+(sim?"_simulation":"");
            assert(std::string(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(device,"identifiers"),0)->valuestring)==expected_id);
            const std::string unique=cJSON_GetObjectItemCaseSensitive(json,"unique_id")->valuestring;
            assert(unique.find(expected_id+"_")==0);
            assert(std::string(cJSON_GetObjectItemCaseSensitive(device,"name")->valuestring)==(sim?"OpenP1 simulation":"OpenP1"));
            cJSON_Delete(json);
        }
        assert(p.payload.find("password")==std::string::npos);
    }
    assert(online_messages==6 && discovery>=6 &&normal_states>0&&simulation_states>0);
    assert(text_raw==9&&text_apdu==3&&text_active>0&&mqtt::text_dropped()==1);
    for(auto* client:fake_clients)delete client;
    for(auto* queue:fake_queues)delete queue;
}
