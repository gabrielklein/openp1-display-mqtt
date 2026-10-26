#include "connectivity/model.hpp"
#include <array>
#include <algorithm>
#include <cassert>
#include <string>
#include <limits>
int main() {
    using namespace openp1::network;
    for (unsigned p=0;p<256;++p) {
        assert(!read_allowed(true,static_cast<std::uint8_t>(p)));
        assert(read_allowed(false,static_cast<std::uint8_t>(p))==(p==1 || p==2));
    }
    assert(credentials_valid("home",""));
    assert(credentials_valid(std::string(32,'a').c_str(),std::string(63,'x').c_str()));
    assert(!credentials_valid("","password"));
    assert(!credentials_valid(std::string(33,'a').c_str(),"password"));
    assert(!credentials_valid("home","short"));
    assert(!credentials_valid("home",std::string(64,'x').c_str()));
    assert(retry_delay(0)==1000 && retry_delay(5)==32000);
    assert(retry_delay(6)==60000 && retry_delay(std::numeric_limits<unsigned>::max())==60000);
    assert(!stale(100,15099) && stale(100,15100) && !stale(100,99));
    std::array<std::uint8_t,512> q{};
    const std::uint8_t query[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,1,0,1};
    const auto n=sizeof(query);
    std::copy(query,query+n,q.begin());
    assert(dns_reply(q.data(),n,q.size())==n+16);
    assert(q[0]==0x12 && q[1]==0x34 && q[n+12]==192 && q[n+15]==1);
    for (std::size_t i=0;i<n;++i) {
        std::copy(query,query+n,q.begin());
        assert(!dns_reply(q.data(),i,q.size()));
    }
    std::copy(query,query+n,q.begin()); assert(!dns_reply(q.data(),n,n+15));
    assert(!dns_reply(q.data(),n,n-1));
    q[12]=0xc0; assert(!dns_reply(q.data(),n,q.size()));
    std::copy(query,query+n,q.begin()); q[16]=28; assert(!dns_reply(q.data(),n,q.size()));
    std::copy(query,query+n,q.begin()); q[5]=2; assert(!dns_reply(q.data(),n,q.size()));
    // Deterministic malformed DNS corpus must never write outside the supplied buffer.
    unsigned state=17;
    for (unsigned trial=0;trial<10000;++trial) {
        for (auto& b:q) { state=state*1664525u+1013904223u; b=state>>24; }
        const auto length=state%513;
        const auto result=dns_reply(q.data(),length,q.size());
        assert(!result || result<=q.size());
    }
}
