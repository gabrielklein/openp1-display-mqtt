#include "support/memory.hpp"
#include "connectivity/internal.hpp"
#include "connectivity/model.hpp"
#include "../../web/pages.hpp"
#include "../../web/access.hpp"
#include "../../web/diagnostics.hpp"
#include "cJSON.h"
#include "meter/presentation.hpp"
#include <memory>
#include <new>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "display/dashboard.hpp"
#include "stream.hpp"
#include "request.hpp"
#include <cstdio>
#include <cstring>
namespace openp1::network::detail {
esp_err_t reply(httpd_req_t* r, const char* code, const char* body, const char* type) {
    if (code[0]>='4' && !std::strcmp(body,"{}"))
        body="{\"error\":{\"code\":\"request_rejected\",\"message\":\"Request unavailable or invalid\"}}";
    httpd_resp_set_status(r,code);
    httpd_resp_set_type(r,type);
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    httpd_resp_set_hdr(r,"Connection","close");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; frame-ancestors 'none'; base-uri 'none'");
    return httpd_resp_send(r,body,HTTPD_RESP_USE_STRLEN);
}
esp_err_t denied(httpd_req_t* r) {
    return reply(r,"403 Forbidden","{\"error\":{\"code\":\"forbidden\",\"message\":\"Access denied\"}}");
}
esp_err_t json(httpd_req_t* r, cJSON* root) {
    if (!root) return reply(r,"503 Service Unavailable","{}");
    char* body = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if (!body) return reply(r,"503 Service Unavailable","{}");
    const auto result = reply(r,"200 OK",body); cJSON_free(body); return result;
}
bool permits(std::uint8_t required) { Guard guard; return grant.permits(policy,setup,required,now_ms()); }
bool allowed() { return permits(1); }
bool provisioning() { Guard guard; return setup; }
static esp_err_t get_impl(httpd_req_t* r) {
    // Wildcard handler: every request, including reused connections, checks current state.
    const bool configuring = provisioning();
    const auto* path = r->uri;
    if (configuring) {
        if(!std::strcmp(path,"/access-check"))return reply(r,"403 Forbidden","","text/plain");
        if (!std::strncmp(path,"/api/",5)) return denied(r);
        char host[64]{};
        if (httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK ||
            (std::strcmp(host,"192.168.4.1") && std::strcmp(host,"192.168.4.1:80"))) {
            httpd_resp_set_hdr(r,"Location","http://192.168.4.1/setup");
            return reply(r,"302 Found","OpenP1 Wi-Fi setup: http://192.168.4.1/setup","text/plain");
        }
        if (!std::strcmp(path,"/setup/token")) {
            auto* root = cJSON_CreateObject(); cJSON_AddStringToObject(root,"token",csrf);
            return json(r,root);
        }
        if (!std::strcmp(path,"/setup/status")) {
            SetupState state; char address[16];
            { Guard guard; state=setup_state; std::memcpy(address,ip,sizeof(ip)); }
            const char* name="idle";
            switch (state) {
            case SetupState::Connecting: name="connecting"; break;
            case SetupState::Saving: name="saving"; break;
            case SetupState::Success: name="success"; break;
            case SetupState::Failed: name="failed"; break;
            case SetupState::Timeout: name="timeout"; break;
            case SetupState::StorageError: name="storage_error"; break;
            case SetupState::Idle: break;
            }
            auto* root=cJSON_CreateObject(); cJSON_AddStringToObject(root,"state",name);
            if (state==SetupState::Success) cJSON_AddStringToObject(root,"ip",address);
            return json(r,root);
        }
        if (!std::strcmp(path,"/setup/scan")) {
            Scan copy; bool busy;
            { Guard guard;
                busy=trying || pending;
                if (!busy && scan.state!=ScanState::Queued && scan.state!=ScanState::Running &&
                    now_ms()>=scan.refresh_after) {
                    scan.state=ScanState::Queued; scan.refresh_after=now_ms()+10000;
                }
                copy=scan;
            }
            if (busy) return reply(r,"409 Conflict","{}");
            if (copy.state==ScanState::Failed) return reply(r,"503 Service Unavailable","{}");
            if (copy.state!=ScanState::Ready)
                return reply(r,"202 Accepted","{\"state\":\"scanning\"}");
            auto* root=cJSON_CreateObject(); cJSON_AddStringToObject(root,"state","ready");
            auto* list=cJSON_AddArrayToObject(root,"networks");
            for (std::size_t i=0;i<copy.count;++i) {
                auto* item=cJSON_CreateObject();
                cJSON_AddStringToObject(item,"ssid",copy.networks[i].ssid);
                cJSON_AddNumberToObject(item,"rssi",copy.networks[i].rssi);
                cJSON_AddBoolToObject(item,"secure",copy.networks[i].secure);
                cJSON_AddItemToArray(list,item);
            }
            return json(r,root);
        }
        if (!std::strcmp(path,"/setup") || !std::strcmp(path,"/"))
            return reply(r,"200 OK",setup_page,"text/html; charset=utf-8");
        httpd_resp_set_hdr(r,"Location","http://192.168.4.1/setup");
        return reply(r,"302 Found","OpenP1 Wi-Fi setup: http://192.168.4.1/setup","text/plain");
    }
    // Public waiting shell and status-only probe expose no config or diagnostics.
    if(!std::strcmp(path,"/access-check"))return reply(r,permits(2)?"204 No Content":"403 Forbidden","","text/plain");
    if(!std::strcmp(path,"/unlock"))return reply(r,"200 OK",access_page,"text/html; charset=utf-8");
    if(((!std::strcmp(path,"/")||!std::strcmp(path,"/diagnostics"))&&!allowed())||(!std::strcmp(path,"/settings")&&!permits(2)))
        return reply(r,"403 Forbidden",access_page,"text/html; charset=utf-8");
    if (!allowed()) return denied(r);
    if(!std::strcmp(path,"/settings") || !std::strcmp(path,"/api/v1/access") ||
       !std::strcmp(path,"/api/v1/firmware") || !std::strcmp(path,"/api/v1/serial") || !std::strcmp(path,"/api/v1/simulation") || !std::strcmp(path,"/api/v1/mqtt/status") || !std::strcmp(path,"/api/v1/display") || !std::strcmp(path,"/api/v1/session") || !std::strcmp(path,"/api/v1/mqtt"))return admin_get(r);
    if(!std::strcmp(path,"/api/v1/events"))return stream_open(r);
    if(!std::strcmp(path,"/diagnostics"))return reply(r,"200 OK",diagnostics_page,"text/html; charset=utf-8");
    if (!std::strcmp(path,"/")) return reply(r,"200 OK",dashboard,"text/html; charset=utf-8");
    if(!std::strcmp(path,"/api/v1/raw")){
        RawHistory history;{Guard guard;history=raw_history;}
        using Json=std::unique_ptr<cJSON,decltype(&cJSON_Delete)>;
        Json root(cJSON_CreateObject(),cJSON_Delete);
        auto* records=root?cJSON_AddArrayToObject(root.get(),"records"):nullptr;
        if(!records||!cJSON_AddNumberToObject(root.get(),"history_overwritten",static_cast<double>(history.overwritten))||!cJSON_AddNumberToObject(root.get(),"limit",4))return reply(r,"503 Service Unavailable","{}");
        for(std::size_t i=0;i<history.count;++i){
            const auto& chunk=history.chunks[i];Json item(cJSON_CreateObject(),cJSON_Delete);
            if(chunk.size>chunk.bytes.size()||!item||!cJSON_AddNumberToObject(item.get(),"sequence",static_cast<double>(history.sequences[i]))||
                !cJSON_AddNumberToObject(item.get(),"timestamp_ms",static_cast<double>(chunk.t_ms))||
                !cJSON_AddStringToObject(item.get(),"source",chunk.source==input::Source::Replay?"replay":"uart")||
                !cJSON_AddStringToObject(item.get(),"layer",chunk.layer==input::Layer::WireHdlc?"wire_hdlc":"llc_apdu"))return reply(r,"503 Service Unavailable","{}");
            char hex[513];constexpr char digits[]="0123456789ABCDEF";
            for(std::size_t n=0;n<chunk.size;++n){hex[n*2]=digits[chunk.bytes[n]>>4];hex[n*2+1]=digits[chunk.bytes[n]&15];}
            hex[chunk.size*2]=0;
            if(!cJSON_AddStringToObject(item.get(),"hex",hex)||!cJSON_AddItemToArray(records,item.get()))return reply(r,"503 Service Unavailable","{}");
            item.release();
        }
        return json(r,root.release());
    }

    if (std::strcmp(path,"/api/v1/status") && std::strcmp(path,"/api/v1/values"))
        return reply(r,"404 Not Found","{\"error\":{\"code\":\"not_found\",\"message\":\"Unknown route\"}}");
    const bool with_values=!std::strcmp(path,"/api/v1/values");
    auto copied=with_values?memory::make<Snapshot>():memory::Pointer<Snapshot>{};
    if(with_values&&!copied)return reply(r,"503 Service Unavailable","{\"error\":{\"code\":\"snapshot_memory\",\"message\":\"Not enough memory to copy readings\"}}");
    const auto now=now_ms();
    bool online,simulation,all_stale=true; char address[16];meter::Counters counters;
    { Guard guard;
        if(copied)*copied=snapshot;
        online=connected;simulation=snapshot.simulation;counters=snapshot.counters;
        std::memcpy(address,ip,sizeof(ip));
        for(std::size_t i=0;i<snapshot.count;++i)if(!stale(snapshot.values[i].t_ms,now))all_stale=false;
    }
    auto* root = cJSON_CreateObject();
    std::unique_ptr<cJSON,decltype(&cJSON_Delete)> root_guard(root,cJSON_Delete);
    cJSON_AddStringToObject(root,"firmware",esp_app_get_description()->version);
    cJSON_AddNumberToObject(root,"uptime_ms",static_cast<double>(now));
    cJSON_AddNumberToObject(root,"free_heap_bytes",static_cast<double>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));
    cJSON_AddNumberToObject(root,"largest_free_block_bytes",static_cast<double>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    cJSON_AddNumberToObject(root,"minimum_free_heap_bytes",static_cast<double>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
    cJSON_AddBoolToObject(root,"connected",online);
    wifi_ap_record_t access_point{};
    if(online&&esp_wifi_sta_get_ap_info(&access_point)==ESP_OK)cJSON_AddNumberToObject(root,"wifi_rssi_dbm",access_point.rssi);
    else cJSON_AddNullToObject(root,"wifi_rssi_dbm");
    cJSON_AddNumberToObject(root,"gauge_limit_w",dashboard::limit_w.load());

    cJSON_AddStringToObject(root,"ip",address);
    cJSON_AddBoolToObject(root,"simulation",simulation);
    {Guard guard;cJSON_AddStringToObject(root,"access",access::name(grant.effective(policy,now)));
        cJSON_AddNumberToObject(root,"access_remaining_seconds",grant.remaining(now));}
    cJSON_AddNumberToObject(root,"errors",static_cast<double>(counters.errors));
    cJSON_AddStringToObject(root,"last_error",meter::error_name(counters.last_error));
    cJSON_AddNumberToObject(root,"last_error_ms",static_cast<double>(counters.error_ms));
    cJSON_AddNumberToObject(root,"validated_hdlc",static_cast<double>(counters.validated_hdlc));
    cJSON_AddNumberToObject(root,"notifications",static_cast<double>(counters.notifications));
    cJSON_AddBoolToObject(root,"stale",all_stale);
    if (with_values) {
        const auto& copy=*copied;
        auto* values = cJSON_AddArrayToObject(root,"values");
        for (std::size_t i=0;i<copy.count;++i) {
            const auto& v = copy.values[i];
            auto* item = cJSON_CreateObject();
            std::unique_ptr<cJSON,decltype(&cJSON_Delete)> item_guard(item,cJSON_Delete);
            char obis[32], unit[16];
            std::snprintf(obis,sizeof(obis),"%u.%u.%u.%u.%u.%u",v.obis[0],v.obis[1],v.obis[2],v.obis[3],v.obis[4],v.obis[5]);
            const auto name=meter::reading_label(v.obis);
            const auto* u=v.has_scaler_unit ? meter::unit_name(v.unit) : "RAW";
            std::snprintf(unit,sizeof(unit),"U%u",v.unit);
            cJSON_AddStringToObject(item,"obis",obis);
            cJSON_AddStringToObject(item,"name",name.c_str());
            cJSON_AddStringToObject(item,"obis_display",meter::obis_display(v.obis).c_str());
            if(v.numeric) cJSON_AddNumberToObject(item,"value",v.has_scaler_unit?v.scaled:v.raw_number);
            else {
                std::string text;bool binary=false;
                const auto offset=copy.offsets[i],length=copy.lengths[i];
                if(offset>copy.encoded_size || length>copy.encoded_size-offset ||
                    !meter::string_value(copy.encoded.data()+offset,length,text,binary)) {
                    continue;
                }
                cJSON_AddStringToObject(item,"value",text.c_str());
                cJSON_AddStringToObject(item,"value_encoding",binary?"hex":"utf8");
            }
            cJSON_AddStringToObject(item,"unit",v.numeric?(u?u:unit):"");
            cJSON_AddNumberToObject(item,"type",v.type);
            if(v.numeric) cJSON_AddNumberToObject(item,"raw_value",v.raw_number);
            cJSON_AddBoolToObject(item,"has_scaler_unit",v.has_scaler_unit);
            if (v.has_scaler_unit) {
                cJSON_AddNumberToObject(item,"scaler",v.scaler);
                cJSON_AddNumberToObject(item,"unit_code",v.unit);
            }
            cJSON_AddNumberToObject(item,"timestamp_ms",static_cast<double>(v.t_ms));
            cJSON_AddNumberToObject(item,"age_ms",static_cast<double>(now>=v.t_ms?now-v.t_ms:0));
            cJSON_AddBoolToObject(item,"stale",stale(v.t_ms,now));
            if(!cJSON_AddItemToArray(values,item))return reply(r,"503 Service Unavailable","{}");
            item_guard.release();
        }
    }
    return json(r,root_guard.release());
}
static esp_err_t post_impl(httpd_req_t* r) {
    if (!provisioning() || std::strcmp(r->uri,"/setup/wifi")) return denied(r);
    char token[33]{}, origin[80]{}, host[64]{}, type[40]{};
    if (httpd_req_get_hdr_value_str(r,"X-Setup-Token",token,sizeof(token)) != ESP_OK || std::strcmp(token,csrf) ||
        httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host)) != ESP_OK ||
        (std::strcmp(host,"192.168.4.1") && std::strcmp(host,"192.168.4.1:80")) ||
        httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type)) != ESP_OK || std::strcmp(type,"application/json")) return denied(r);
    if (httpd_req_get_hdr_value_len(r,"Origin")) {
        if (httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin)) != ESP_OK ||
            (std::strcmp(origin,"http://192.168.4.1") && std::strcmp(origin,"http://192.168.4.1:80"))) return denied(r);
    }
    if (!r->content_len || r->content_len > 384) return reply(r,"400 Bad Request","{}");
    char body[385]{}; std::size_t offset=0;
    const auto receive_deadline=now_ms()+3000;
    while (offset < r->content_len) {
        const int n=httpd_req_recv(r,body+offset,r->content_len-offset);
        if (n<=0 || now_ms()>receive_deadline) { std::memset(body,0,sizeof(body)); return ESP_FAIL; }
        offset+=static_cast<std::size_t>(n);
    }
    if (!http::shallow_json(body,offset)) { std::memset(body,0,sizeof(body)); return reply(r,"400 Bad Request","{}"); }
    const char* end=nullptr;
    auto* root=cJSON_ParseWithLengthOpts(body,offset+1,&end,true);
    auto* ssid=cJSON_GetObjectItemCaseSensitive(root,"ssid");
    auto* pass=cJSON_GetObjectItemCaseSensitive(root,"password");
    const bool valid=cJSON_IsObject(root) && cJSON_GetArraySize(root)==2 && cJSON_IsString(ssid) && cJSON_IsString(pass) &&
        credentials_valid(ssid->valuestring,pass->valuestring) && !std::strstr(body,"\\u0000");
    if (!valid) { cJSON_Delete(root); std::memset(body,0,sizeof(body)); return reply(r,"400 Bad Request","{}"); }
    bool busy;
    { Guard guard; busy=pending || trying || scan.state==ScanState::Queued || scan.state==ScanState::Running; if (!busy) {
        candidate={}; std::memcpy(candidate.sta.ssid,ssid->valuestring,std::strlen(ssid->valuestring));
        std::memcpy(candidate.sta.password,pass->valuestring,std::strlen(pass->valuestring));
        setup_state=SetupState::Connecting; pending=true;
    } }
    cJSON_Delete(root); std::memset(body,0,sizeof(body));
    return reply(r,busy?"409 Conflict":"202 Accepted","{}");
}
esp_err_t get(httpd_req_t* r){
    try{return get_impl(r);}catch(const std::bad_alloc&){return reply(r,"503 Service Unavailable","{}");}
}
esp_err_t post(httpd_req_t* r){
    try{return post_impl(r);}catch(const std::bad_alloc&){return reply(r,"503 Service Unavailable","{}");}
}
}
