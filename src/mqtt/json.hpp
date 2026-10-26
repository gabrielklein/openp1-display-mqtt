#pragma once
#include "config.hpp"
#include "cJSON.h"
#include <cmath>
namespace openp1::mqtt {
inline cJSON* config_json(const Config& c) {
    auto* r=cJSON_CreateObject();
    cJSON_AddBoolToObject(r,"enabled",c.enabled);cJSON_AddBoolToObject(r,"tls",c.tls);
    cJSON_AddBoolToObject(r,"discovery",c.discovery);
    cJSON_AddStringToObject(r,"host",c.host);cJSON_AddNumberToObject(r,"port",c.port);
    cJSON_AddStringToObject(r,"root",c.root);cJSON_AddStringToObject(r,"ntp",c.ntp);
    cJSON_AddBoolToObject(r,"state_enabled",c.state_enabled);
    cJSON_AddBoolToObject(r,"obis_enabled",c.obis_enabled);
    cJSON_AddBoolToObject(r,"text_raw_enabled",c.text_raw_enabled);cJSON_AddBoolToObject(r,"text_enabled",c.text_enabled);cJSON_AddStringToObject(r,"text_id",c.text_id);
    cJSON_AddBoolToObject(r,"username_configured",*c.username);
    cJSON_AddBoolToObject(r,"password_configured",*c.password);cJSON_AddBoolToObject(r,"ca_configured",*c.ca);
    return r;
}
inline bool patch_config(const cJSON* r,Config& c) {
    if(!cJSON_IsObject(r)) return false;
    for(const cJSON* p=r->child;p;p=p->next) {
        const auto* key=p->string;if(!key)return false;
        // Reject duplicate fields rather than silently applying the last value.
        for(const cJSON* q=p->next;q;q=q->next) if(q->string&&!std::strcmp(q->string,key))return false;
        std::uint8_t* flag=nullptr;char* text=nullptr;std::size_t capacity=0;
        if(!std::strcmp(key,"enabled"))flag=&c.enabled;
        else if(!std::strcmp(key,"tls"))flag=&c.tls;
        else if(!std::strcmp(key,"discovery"))flag=&c.discovery;
        else if(!std::strcmp(key,"state_enabled"))flag=&c.state_enabled;
        else if(!std::strcmp(key,"obis_enabled"))flag=&c.obis_enabled;
        else if(!std::strcmp(key,"text_raw_enabled"))flag=&c.text_raw_enabled;
        else if(!std::strcmp(key,"text_enabled"))flag=&c.text_enabled;
        else if(!std::strcmp(key,"port")) {
            if(!cJSON_IsNumber(p)||p->valuedouble<1||p->valuedouble>65535||std::floor(p->valuedouble)!=p->valuedouble)return false;
            c.port=static_cast<std::uint16_t>(p->valuedouble);continue;
        } else {
#define OPENP1_FIELD(field) if(!std::strcmp(key,#field)){text=c.field;capacity=sizeof(c.field);}
            OPENP1_FIELD(host) else OPENP1_FIELD(username) else OPENP1_FIELD(password)
            else OPENP1_FIELD(root) else OPENP1_FIELD(ca) else OPENP1_FIELD(ntp) else OPENP1_FIELD(text_id)
#undef OPENP1_FIELD
            else return false;
        }
        if(flag){if(!cJSON_IsBool(p))return false;*flag=cJSON_IsTrue(p);}
        else {if(!cJSON_IsString(p)||std::strlen(p->valuestring)>=capacity)return false;std::strcpy(text,p->valuestring);}
    }
    return valid(c);
}
}
