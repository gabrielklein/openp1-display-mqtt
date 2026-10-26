#pragma once
#include "meter/presentation.hpp"
#include "cJSON.h"
#include <cstdio>
#include <memory>
#include <new>
namespace openp1::mqtt {
inline void key_into(const meter::Reading& r,char* s,std::size_t capacity) {
    std::snprintf(s,capacity,"%u_%u_%u_%u_%u_%u_a%u",r.obis[0],r.obis[1],r.obis[2],r.obis[3],r.obis[4],r.obis[5],r.attribute);
}
inline std::string key(const meter::Reading& r){char s[40];key_into(r,s,sizeof(s));return s;}
using Json=std::unique_ptr<cJSON,decltype(&cJSON_Delete)>;
inline cJSON* state_json(const meter::PresentationSnapshot& s,std::uint64_t now) noexcept {
    try{
        Json root(cJSON_CreateObject(),cJSON_Delete);
        if(!root||!cJSON_AddNumberToObject(root.get(),"uptime_ms",static_cast<double>(now))||
            !cJSON_AddBoolToObject(root.get(),"simulation",s.simulation)||
            !cJSON_AddNumberToObject(root.get(),"errors",static_cast<double>(s.counters.errors)))return nullptr;
        auto* values=cJSON_AddObjectToObject(root.get(),"values");if(!values)return nullptr;
        for(std::size_t i=0;i<s.count;++i){
            const auto& r=s.values[i];Json v(cJSON_CreateObject(),cJSON_Delete);if(!v)return nullptr;
            if(r.numeric){if(!cJSON_AddNumberToObject(v.get(),"value",r.has_scaler_unit?r.scaled:r.raw_number))return nullptr;}
            else{
                std::string text;bool binary;
                if(s.offsets[i]>s.encoded_size||s.lengths[i]>s.encoded_size-s.offsets[i]||
                    !meter::string_value(s.encoded.data()+s.offsets[i],s.lengths[i],text,binary))continue;
                if(!cJSON_AddStringToObject(v.get(),"value",text.c_str())||
                    !cJSON_AddStringToObject(v.get(),"encoding",binary?"hex":"utf8"))return nullptr;
            }
            if(!cJSON_AddBoolToObject(v.get(),"stale",now>=r.t_ms&&now-r.t_ms>=15000)||
                !cJSON_AddNumberToObject(v.get(),"timestamp_ms",static_cast<double>(r.t_ms))||
                !cJSON_AddNumberToObject(v.get(),"type",r.type)||!cJSON_AddBoolToObject(v.get(),"has_scaler_unit",r.has_scaler_unit))return nullptr;
            if(r.has_scaler_unit&&(!cJSON_AddNumberToObject(v.get(),"scaler",r.scaler)||!cJSON_AddNumberToObject(v.get(),"unit_code",r.unit)))return nullptr;
            char object_key[40];key_into(r,object_key,sizeof(object_key));
            if(!cJSON_AddItemToObject(values,object_key,v.get()))return nullptr;
            v.release();
        }
        return root.release();
    }catch(const std::bad_alloc&){return nullptr;}
}
inline cJSON* discovery_json(const meter::Reading& r,const char* root,const char* device,bool simulation=false) {
    const auto k=key(r),base=std::string(root);
    Json j(cJSON_CreateObject(),cJSON_Delete);if(!j)return nullptr;
    auto add=[&](const char* field,const std::string& value){return cJSON_AddStringToObject(j.get(),field,value.c_str())!=nullptr;};
    if(!add("unique_id",std::string("openp1_")+device+'_'+k)||
        !add("name",meter::reading_label(r.obis)+(simulation?" (simulation)":""))||
        !add("state_topic",base+"/state")||!add("value_template","{{ value_json.values['"+k+"'].value }}")||
        !cJSON_AddNumberToObject(j.get(),"expire_after",30))return nullptr;
    auto* avail=cJSON_AddArrayToObject(j.get(),"availability");if(!avail)return nullptr;
    for(unsigned i=0;i<2;++i){
        Json a(cJSON_CreateObject(),cJSON_Delete);if(!a)return nullptr;
        if(!cJSON_AddStringToObject(a.get(),"topic",(base+(i?"/state":"/availability")).c_str()))return nullptr;
        if(i&&!cJSON_AddStringToObject(a.get(),"value_template",("{{ 'offline' if value_json.values.get('"+k+"', {}).get('stale', true) else 'online' }}").c_str()))return nullptr;
        if(!cJSON_AddItemToArray(avail,a.get()))return nullptr;
        a.release();
    }
    if(!add("availability_mode","all"))return nullptr;
    if(r.has_scaler_unit){
        const char* unit=nullptr;const char* device_class=nullptr;
        switch(r.unit){case 27:unit="W";device_class="power";break;case 30:unit="Wh";device_class="energy";break;
            case 33:unit="A";device_class="current";break;case 35:unit="V";device_class="voltage";break;case 44:unit="Hz";device_class="frequency";break;}
        if(unit){
            const bool energy=r.unit==30&&r.obis[0]==1&&r.obis[3]==8&&(r.obis[2]==1||r.obis[2]==2);
            if(!add("unit_of_measurement",unit)||!add("device_class",device_class)||!add("state_class",energy?"total_increasing":"measurement"))return nullptr;
        }
    }
    auto* d=cJSON_AddObjectToObject(j.get(),"device");if(!d)return nullptr;
    auto* ids=cJSON_AddArrayToObject(d,"identifiers");if(!ids)return nullptr;
    Json id(cJSON_CreateString((std::string("openp1_")+device).c_str()),cJSON_Delete);
    if(!id||!cJSON_AddItemToArray(ids,id.get()))return nullptr;
    id.release();
    if(!cJSON_AddStringToObject(d,"name",simulation?"OpenP1 simulation":"OpenP1"))return nullptr;
    return j.release();
}
}
