#pragma once
#include "protocol.hpp"
#include <cstdio>
#include <cstring>
#include <string>
namespace openp1::meter {
// Fixed cross-task presentation copy: total exact A-XDR values remain <= 8192 bytes.
struct PresentationSnapshot {
    std::array<Reading,64> values{};
    std::array<std::uint8_t,8192> encoded{};
    std::array<std::uint16_t,64> offsets{},lengths{};
    std::size_t encoded_size=0,count=0;
    Counters counters{};
    bool simulation=false;
    void capture(const Processor& processor,bool replay) {
        count=0;encoded_size=0;simulation=replay;counters=processor.counters();
        for(const auto& sample:processor.readings()) {
            const auto& r=sample.reading;
            if(!r.numeric && r.type!=9 && r.type!=10 && r.type!=12) continue;
            if(count>=values.size() || sample.encoded_value.size()>encoded.size()-encoded_size) break;
            const auto i=count++;
            values[i]=r;offsets[i]=encoded_size;lengths[i]=sample.encoded_value.size();
            std::memcpy(encoded.data()+encoded_size,sample.encoded_value.data(),sample.encoded_value.size());
            encoded_size+=sample.encoded_value.size();
        }
    }
};
inline std::string obis_display(const std::array<std::uint8_t,6>& o) {
    char out[32];
    std::snprintf(out,sizeof(out),"%u-%u:%u.%u.%u",o[0],o[1],o[2],o[3],o[4]);
    std::string result=out;
    if(o[5]!=255) result+='*'+std::to_string(o[5]);
    return result;
}
inline std::string reading_label(const std::array<std::uint8_t,6>& o) {
    if(o[0]==0 && o[2]==96 && o[3]==1 && o[5]==255) {
        if(o[4]==0) return "Serial number";
        if(o[4]==1) return "GR number";
    }
    if(o[0]==1 && o[3]==8 && (o[2]==1 || o[2]==2)) {
        std::string name=o[2]==1?"Import +A":"Export -A";
        return name+(o[4]==0?" total":" tariff "+std::to_string(o[4]));
    }
    const auto* name=known_name(o); return name?name:"Unknown object";
}
// A-XDR string bytes remain exact. Only valid printable UTF-8 becomes JSON text;
// binary/control-containing identifiers use a lossless hex representation.
inline bool string_value(const std::uint8_t* bytes,std::size_t size,std::string& out,bool& binary) {
    if(size<2 || (bytes[0]!=9 && bytes[0]!=10 && bytes[0]!=12)) return false;
    std::size_t pos=2, length=bytes[1];
    if(length&128) {
        const auto count=length&127;length=0;
        if(!count || count>4 || count>size-pos) return false;
        for(std::size_t i=0;i<count;++i) length=(length<<8)|bytes[pos++];
    }
    if(length>1024 || length!=size-pos) return false;
    binary=false;
    for(std::size_t i=pos;i<size;) {
        const auto c=bytes[i++];
        if(c<32 || c==127) {binary=true;break;}
        if(c<128) continue;
        unsigned count=0;std::uint32_t code=0, minimum=0;
        if(c>=0xc2 && c<=0xdf){count=1;code=c&31;minimum=0x80;}
        else if(c>=0xe0 && c<=0xef){count=2;code=c&15;minimum=0x800;}
        else if(c>=0xf0 && c<=0xf4){count=3;code=c&7;minimum=0x10000;}
        else {binary=true;break;}
        if(count>size-i){binary=true;break;}
        for(unsigned j=0;j<count;++j) {
            const auto next=bytes[i++];if((next&0xc0)!=0x80)binary=true;
            code=(code<<6)|(next&63);
        }
        if(binary || code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff)){binary=true;break;}
    }
    out.clear();
    if(binary) {
        constexpr char hex[]="0123456789ABCDEF";
        for(std::size_t i=pos;i<size;++i){out+=hex[bytes[i]>>4];out+=hex[bytes[i]&15];}
    } else out.assign(reinterpret_cast<const char*>(bytes+pos),length);
    return true;
}
}
