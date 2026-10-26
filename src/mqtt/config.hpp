#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>
namespace openp1::mqtt {
struct Config {
    std::uint32_t version=5;
    std::uint8_t enabled=0,tls=0,discovery=0;
    std::uint8_t text_raw_enabled=1; // Independent hexadecimal frame/APDU output.
    std::uint16_t port=1883;
    char host[129]{},username[65]{},password[129]{},root[97]{},ca[3073]{};
    char ntp[129]="pool.ntp.org";
    std::uint8_t text_enabled=1;
    char text_id[33]="ISK"; // User-supplied compatibility label, not detected manufacturer.
    std::uint8_t obis_enabled=0;
    std::uint8_t state_enabled=1;
};
inline constexpr std::size_t config_v1_size=offsetof(Config,text_enabled);
static_assert(sizeof(Config)==3668,"Preserve the version-2 NVS size");
static_assert(config_v1_size==3632,"Preserve the version-1 NVS layout");
inline bool host_valid(const char* s) {
    if(!*s) return false;
    for(std::size_t i=0;s[i];++i) {
        const auto c=static_cast<unsigned char>(s[i]);
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c==':')) return false;
    }
    return true;
}
inline bool valid(const Config& c) {
    if(c.version!=5 || c.state_enabled>1 || c.obis_enabled>1 || c.text_raw_enabled>1 || c.text_enabled>1 || !std::memchr(c.text_id,0,sizeof(c.text_id)) || c.enabled>1 || c.tls>1 || c.discovery>1 || !c.port || !std::memchr(c.host,0,sizeof(c.host)) ||
       !std::memchr(c.username,0,sizeof(c.username)) || !std::memchr(c.password,0,sizeof(c.password)) ||
       !std::memchr(c.root,0,sizeof(c.root)) || !std::memchr(c.ca,0,sizeof(c.ca)) || !std::memchr(c.ntp,0,sizeof(c.ntp))) return false;
    for(const char* p=c.text_id;*p;++p)if(static_cast<unsigned char>(*p)<32||static_cast<unsigned char>(*p)>126)return false;
    if((c.enabled && !host_valid(c.host)) || (*c.ntp && !host_valid(c.ntp))) return false;
    for(const char* p=c.root;*p;++p) if(static_cast<unsigned char>(*p)<33 || *p=='+'||*p=='#') return false;
    const auto length=std::strlen(c.root);
    if(length && c.root[length-1]=='/') return false;
    if(*c.ca && (!std::strstr(c.ca,"-----BEGIN CERTIFICATE-----") || !std::strstr(c.ca,"-----END CERTIFICATE-----"))) return false;
    return true;
}
inline bool may_publish(bool enabled,bool station,bool setup) {
    return enabled && station && !setup;
}
}
