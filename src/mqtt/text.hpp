#pragma once
#include "meter/presentation.hpp"
#include <cmath>
#include <cstdio>
namespace openp1::mqtt {
// One fresh attribute-2 value per OBIS topic; scaler applied, no unit suffix.
inline bool obis_payload(const meter::PresentationSnapshot& s,std::size_t index,std::uint64_t now,char* out,std::size_t capacity){
    if(index>=s.count||!capacity)return false;
    const auto& r=s.values[index];
    if(r.attribute!=2||now<r.t_ms||now-r.t_ms>=15000)return false;
    if(r.numeric){const double value=r.has_scaler_unit?r.scaled:r.raw_number;if(!std::isfinite(value))return false;
        const int n=std::snprintf(out,capacity,"%.15g",value);return n>=0&&static_cast<std::size_t>(n)<capacity;}
    const auto offset=s.offsets[index],size=s.lengths[index];
    if(offset>s.encoded_size||size>s.encoded_size-offset)return false;
    std::string text;bool binary=false;
    if(!meter::string_value(s.encoded.data()+offset,size,text,binary)||text.size()>=capacity)return false;
    std::memcpy(out,text.data(),text.size());out[text.size()]=0;return true;
}
// Compatibility summaries are derived output, never additional meter readings.
inline bool text_summary(const meter::PresentationSnapshot& s,std::uint64_t now,const char* group,char* out,std::size_t capacity){
    struct Key {unsigned c,d,e,unit;};
    Key keys[4]{};unsigned count=0;
    if(!std::strcmp(group,"active")){keys[0]={1,7,0,27};keys[1]={2,7,0,27};keys[2]={1,8,0,30};keys[3]={2,8,0,30};count=4;}
    else if(!std::strcmp(group,"voltage")){keys[0]={32,7,0,35};keys[1]={52,7,0,35};keys[2]={72,7,0,35};count=3;}
    else if(!std::strcmp(group,"current")){keys[0]={31,7,0,33};keys[1]={51,7,0,33};keys[2]={71,7,0,33};count=3;}
    else if(!std::strcmp(group,"metering")){keys[0]={1,8,1,30};keys[1]={1,8,2,30};keys[2]={2,8,1,30};keys[3]={2,8,2,30};count=4;}
    else return false;
    double values[4]{};std::uint64_t timestamp=0;
    for(unsigned k=0;k<count;++k){
        bool found=false;
        for(std::size_t i=0;i<s.count;++i){const auto& r=s.values[i];
            if(r.obis!=std::array<std::uint8_t,6>{1,0,static_cast<std::uint8_t>(keys[k].c),static_cast<std::uint8_t>(keys[k].d),static_cast<std::uint8_t>(keys[k].e),255}||r.attribute!=2)continue;
            if(!r.numeric||!r.has_scaler_unit||r.unit!=keys[k].unit||!std::isfinite(r.scaled)||now<r.t_ms||now-r.t_ms>=15000||(k&&r.t_ms!=timestamp))return false;
            timestamp=r.t_ms;values[k]=r.scaled;found=true;break;
        }
        if(!found)return false;
    }
    if((!std::strcmp(group,"voltage")||!std::strcmp(group,"current"))&&!std::isfinite(values[0]+values[1]+values[2]))return false;
    int n=-1;
    if(!std::strcmp(group,"active"))n=std::snprintf(out,capacity,"%.15g %.15g %.2f %.2f",values[0],values[1],values[2]/1000,values[3]/1000);
    else if(!std::strcmp(group,"voltage"))n=std::snprintf(out,capacity,"%.2f %.2f %.2f %.2f",(values[0]+values[1]+values[2])/3,values[0],values[1],values[2]);
    else if(!std::strcmp(group,"current"))n=std::snprintf(out,capacity,"%.2f %.2f %.2f %.2f",values[0]+values[1]+values[2],values[0],values[1],values[2]);
    else n=std::snprintf(out,capacity,"%.15g %.15g %.15g %.15g",values[0],values[1],values[2],values[3]);
    return n>=0&&static_cast<std::size_t>(n)<capacity;
}
}
