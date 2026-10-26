#include "simulation/service.hpp"
#include "support/memory.hpp"
#include "connectivity/internal.hpp"
#include "request.hpp"
#include "config/serial.hpp"
#include "display/dashboard.hpp"
#include "mqtt/service.hpp"
#include "mqtt/json.hpp"
#include "../../web/settings.hpp"
#include "nvs.h"
#include <memory>
#include <new>
namespace openp1::network::detail {
bool same_origin(httpd_req_t* r,bool token,const char* content_type){
    char address[16];{Guard guard;std::memcpy(address,ip,sizeof(ip));}
    char host[64]{},origin[80]{},csrf_header[33]{},type[40]{};
    const auto authority=std::string(address),with_port=authority+":80";
    if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK ||
        (host!=authority && host!=with_port))return false;
    if(httpd_req_get_hdr_value_len(r,"Origin") &&
       (httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK ||
        (origin!="http://"+authority && origin!="http://"+with_port)))return false;
    if(token && (httpd_req_get_hdr_value_str(r,"X-CSRF-Token",csrf_header,sizeof(csrf_header))!=ESP_OK ||
        std::strcmp(csrf_header,csrf) || httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK ||
        std::strcmp(type,content_type)))return false;
    return true;
}
esp_err_t admin_get(httpd_req_t* r){
    if(!std::strcmp(r->uri,"/api/v1/access")){
        if(!permits(1))return denied(r);
        auto* root=cJSON_CreateObject();
        {Guard guard;cJSON_AddStringToObject(root,"persisted",access::name(policy));
            cJSON_AddStringToObject(root,"effective",access::name(grant.effective(policy,now_ms())));
            cJSON_AddNumberToObject(root,"remaining_seconds",grant.remaining(now_ms()));}
        return json(r,root);
    }
    if(!permits(2))return reply(r,"403 Forbidden","{\"error\":{\"code\":\"full_required\",\"message\":\"Full access required: press and release S2, then retry.\"}}");
    if(!same_origin(r,false))return reply(r,"403 Forbidden","{\"error\":{\"code\":\"station_origin_required\",\"message\":\"Use the device IP shown on its display in a regular browser.\"}}");
    if(!std::strcmp(r->uri,"/api/v1/firmware"))return firmware_get(r);
    if(!std::strcmp(r->uri,"/settings"))return reply(r,"200 OK",settings_page,"text/html; charset=utf-8");
    if(!std::strcmp(r->uri,"/api/v1/session")){
        auto* root=cJSON_CreateObject();cJSON_AddStringToObject(root,"token",csrf);return json(r,root);
    }
    if(!std::strcmp(r->uri,"/api/v1/serial")){
        input::SerialSettings config;if(!serial::get(config))return reply(r,"503 Service Unavailable","{}");
        auto* root=cJSON_CreateObject();
        if(!root||!cJSON_AddNumberToObject(root,"baud",config.baud)||!cJSON_AddStringToObject(root,"parity",config.parity==input::Parity::Even?"Even":config.parity==input::Parity::Odd?"Odd":"None")||!cJSON_AddNumberToObject(root,"stop_bits",config.stop_bits==input::StopBits::Two?2:1)||!cJSON_AddBoolToObject(root,"inverted",config.inverted)){cJSON_Delete(root);return reply(r,"503 Service Unavailable","{}");}
        return json(r,root);
    }
    if(!std::strcmp(r->uri,"/api/v1/simulation")){
        auto config=memory::make<input::SimulationConfig>();
        if(!config||!simulation::get(*config))return reply(r,"503 Service Unavailable","{}");
        auto* root=cJSON_CreateObject();
        if(!root||!cJSON_AddBoolToObject(root,"enabled",config->enabled)||
            !cJSON_AddStringToObject(root,"layer",config->layer?"llc_apdu":"wire_hdlc")||
            !cJSON_AddStringToObject(root,"text",config->text)||
            !cJSON_AddStringToObject(root,"default_text",input::default_simulation_text)||
            !cJSON_AddNumberToObject(root,"repeat_pause_ms",3500)){
            cJSON_Delete(root);return reply(r,"503 Service Unavailable","{}");
        }
        return json(r,root);
    }
    if(!std::strcmp(r->uri,"/api/v1/display")){
        auto* root=cJSON_CreateObject();cJSON_AddBoolToObject(root,"rotate_180",display::rotated());cJSON_AddNumberToObject(root,"gauge_limit_w",dashboard::limit_w.load());
        return json(r,root);
    }
    if(!std::strcmp(r->uri,"/api/v1/mqtt/status")){
        const auto stats=mqtt::statistics();auto* root=cJSON_CreateObject();
        if(!root ||
            !cJSON_AddStringToObject(root,"status",mqtt::status()) ||
            !cJSON_AddBoolToObject(root,"connected",stats.connected) ||
            !cJSON_AddNumberToObject(root,"messages_sent",stats.sent) ||
            !cJSON_AddNumberToObject(root,"publish_failures",stats.publish_failures) ||
            !cJSON_AddNumberToObject(root,"errors",stats.errors) ||
            !cJSON_AddStringToObject(root,"last_error",mqtt::error_name(stats.last_error)) ||
            !cJSON_AddNumberToObject(root,"error_type",stats.error_type) ||
            !cJSON_AddNumberToObject(root,"broker_code",stats.broker_code) ||
            !cJSON_AddNumberToObject(root,"tls_code",stats.tls_code) ||
            !cJSON_AddNumberToObject(root,"tls_stack_code",stats.tls_stack_code) ||
            !cJSON_AddNumberToObject(root,"socket_code",stats.socket_code)){cJSON_Delete(root);return reply(r,"503 Service Unavailable","{}");}
        return json(r,root);
    }
    auto config=memory::make<mqtt::Config>();
    if(!config||!mqtt::get(*config))return reply(r,"503 Service Unavailable","{}");
    auto* root=mqtt::config_json(*config);cJSON_AddStringToObject(root,"status",mqtt::status());
    cJSON_AddNumberToObject(root,"text_dropped",mqtt::text_dropped());
    return json(r,root);
}
static esp_err_t put_impl(httpd_req_t* r){
    if(!std::strcmp(r->uri,"/api/v1/firmware"))return firmware_put(r);
    if(!permits(2)||!same_origin(r,true))return denied(r);
    const bool serial_route=!std::strcmp(r->uri,"/api/v1/serial");
    const bool access_route=!std::strcmp(r->uri,"/api/v1/access");
    const bool display_route=!std::strcmp(r->uri,"/api/v1/display");
    const bool simulation_route=!std::strcmp(r->uri,"/api/v1/simulation");
    if(!serial_route && !access_route && !display_route && !simulation_route && std::strcmp(r->uri,"/api/v1/mqtt"))return reply(r,"404 Not Found","{}");
    const std::size_t limit=serial_route?256:(access_route||display_route)?64:8192;
    if(!r->content_len||r->content_len>limit)return reply(r,"400 Bad Request","{}");
    auto body=memory::buffer(r->content_len+1);
    if(!body)return reply(r,"503 Service Unavailable","{}");
    body.get()[r->content_len]=0;
    std::size_t offset=0;const auto deadline=now_ms()+3000;
    while(offset<r->content_len){const int n=httpd_req_recv(r,body.get()+offset,r->content_len-offset);
        if(n<=0||now_ms()>deadline){std::memset(body.get(),0,r->content_len);return ESP_FAIL;}offset+=n;}
    const bool safe=http::shallow_json(body.get(),offset)&&!std::strstr(body.get(),"\\u0000");
    using Json=std::unique_ptr<cJSON,decltype(&cJSON_Delete)>;
    Json root(safe?cJSON_ParseWithLengthOpts(body.get(),offset+1,nullptr,true):nullptr,cJSON_Delete);
    std::memset(body.get(),0,r->content_len);
    if(!root)return reply(r,"400 Bad Request","{}");
    static std::uint64_t write_after=0;
    if(now_ms()<write_after)return reply(r,"429 Too Many Requests","{}");
    if(serial_route){
        input::SerialSettings config;if(!serial::get(config))return reply(r,"503 Service Unavailable","{}");
        bool baud=false,parity=false,stop=false,inverted=false;
        if(!cJSON_IsObject(root.get())||!root->child)return reply(r,"400 Bad Request","{}");
        for(const auto* v=root->child;v;v=v->next){
            if(!v->string)return reply(r,"400 Bad Request","{}");
            if(!std::strcmp(v->string,"baud")&&!baud&&cJSON_IsNumber(v)&&std::isfinite(v->valuedouble)&&v->valuedouble>=300&&v->valuedouble<=115200&&std::floor(v->valuedouble)==v->valuedouble){baud=true;config.baud=static_cast<std::uint32_t>(v->valuedouble);}
            else if(!std::strcmp(v->string,"parity")&&!parity&&cJSON_IsString(v)){
                parity=true;if(!std::strcmp(v->valuestring,"None"))config.parity=input::Parity::None;else if(!std::strcmp(v->valuestring,"Even"))config.parity=input::Parity::Even;else if(!std::strcmp(v->valuestring,"Odd"))config.parity=input::Parity::Odd;else return reply(r,"400 Bad Request","{}");
            }else if(!std::strcmp(v->string,"stop_bits")&&!stop&&cJSON_IsNumber(v)&&(v->valuedouble==1||v->valuedouble==2)){stop=true;config.stop_bits=v->valuedouble==2?input::StopBits::Two:input::StopBits::One;}
            else if(!std::strcmp(v->string,"inverted")&&!inverted&&cJSON_IsBool(v)){inverted=true;config.inverted=cJSON_IsTrue(v);}
            else return reply(r,"400 Bad Request","{}");
        }
        if(!permits(2))return denied(r);
        if(!serial::configure(config))return reply(r,"503 Service Unavailable","{}");
    }else if(simulation_route){
        if(!cJSON_IsObject(root.get()))return reply(r,"400 Bad Request","{}");
        auto config=memory::make<input::SimulationConfig>();
        if(!config||!simulation::get(*config))return reply(r,"503 Service Unavailable","{}");
        bool seen_enabled=false,seen_layer=false,seen_text=false;
        for(auto* item=root->child;item;item=item->next){
            if(!item->string)return reply(r,"400 Bad Request","{}");
            if(!std::strcmp(item->string,"enabled")&&!seen_enabled&&cJSON_IsBool(item)){
                seen_enabled=true;config->enabled=cJSON_IsTrue(item);
            }else if(!std::strcmp(item->string,"layer")&&!seen_layer&&cJSON_IsString(item)){
                seen_layer=true;
                if(!std::strcmp(item->valuestring,"wire_hdlc"))config->layer=0;
                else if(!std::strcmp(item->valuestring,"llc_apdu"))config->layer=1;
                else return reply(r,"400 Bad Request","{}");
            }else if(!std::strcmp(item->string,"text")&&!seen_text&&cJSON_IsString(item)&&
                std::strlen(item->valuestring)<=input::SimulationConfig::max_text){
                seen_text=true;std::strcpy(config->text,item->valuestring);
            }else return reply(r,"400 Bad Request","{}");
        }
        if(!seen_enabled)return reply(r,"400 Bad Request","{}");
        if(!permits(2))return denied(r);
        char error[128]{};
        if(!simulation::configure(*config,error,sizeof(error))){
            if(!error[0])return reply(r,"503 Service Unavailable","{}");
            auto* response=cJSON_CreateObject();auto* detail=cJSON_AddObjectToObject(response,"error");
            cJSON_AddStringToObject(detail,"message",error);
            char* text=cJSON_PrintUnformatted(response);cJSON_Delete(response);
            if(!text)return reply(r,"503 Service Unavailable","{}");
            const auto result=reply(r,"400 Bad Request",text);cJSON_free(text);return result;
        }
    } else if(display_route){
        bool rotate=display::rotated(),seen_rotate=false,seen_limit=false;
        auto limit=dashboard::limit_w.load();
        if(!cJSON_IsObject(root.get())||!root->child)return reply(r,"400 Bad Request","{}");
        for(const auto* value=root->child;value;value=value->next){
            if(value->string&&!std::strcmp(value->string,"rotate_180")&&!seen_rotate&&cJSON_IsBool(value)){seen_rotate=true;rotate=cJSON_IsTrue(value);}
            else if(value->string&&!std::strcmp(value->string,"gauge_limit_w")&&!seen_limit&&cJSON_IsNumber(value)&&std::isfinite(value->valuedouble)&&value->valuedouble>=100&&value->valuedouble<=1000000&&std::floor(value->valuedouble)==value->valuedouble){seen_limit=true;limit=static_cast<std::uint32_t>(value->valuedouble);}
            else return reply(r,"400 Bad Request","{}");
        }
        if(!permits(2))return denied(r);
        nvs_handle_t store;
        if(nvs_open("openp1",NVS_READWRITE,&store)!=ESP_OK)return reply(r,"503 Service Unavailable","{}");
        auto status=nvs_set_u8(store,"display_180",rotate?1:0);
        if(status==ESP_OK)status=nvs_set_blob(store,"gauge_w",&limit,sizeof(limit));
        if(status==ESP_OK)status=nvs_commit(store);
        nvs_close(store);
        if(status!=ESP_OK)return reply(r,"503 Service Unavailable","{}");
        display::set_rotated(rotate);dashboard::limit_w=limit;
    } else if(access_route){
        const auto* p=cJSON_GetObjectItemCaseSensitive(root.get(),"policy");
        if(!cJSON_IsObject(root.get())||cJSON_GetArraySize(root.get())!=1||!cJSON_IsString(p))return reply(r,"400 Bad Request","{}");
        std::uint8_t value=255;for(std::uint8_t i=0;i<3;++i)if(!std::strcmp(p->valuestring,access::name(i)))value=i;
        if(!access::valid(value))return reply(r,"400 Bad Request","{}");
        if(!permits(2))return denied(r);
        if(!save_policy(value))return reply(r,"503 Service Unavailable","{}");

    } else {
        auto config=memory::make<mqtt::Config>();
        if(!config||!mqtt::get(*config))return reply(r,"503 Service Unavailable","{}");
        if(!mqtt::patch_config(root.get(),*config))return reply(r,"400 Bad Request","{}");
        if(!permits(2))return denied(r);
        if(!mqtt::configure(*config))return reply(r,"400 Bad Request","{}");
    }
    write_after=now_ms()+1000;
    const auto result=reply(r,"200 OK","{\"saved\":true}");
    if(result==ESP_OK&&(simulation_route||access_route||serial_route))simulation::request_restart();
    return result;
}
esp_err_t put(httpd_req_t* r){
    try{return put_impl(r);}catch(const std::bad_alloc&){return reply(r,"503 Service Unavailable","{}");}
}
}
