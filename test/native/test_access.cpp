#include "access/control.hpp"
#include "http/request.hpp"
#include <cassert>
#include <string>
int main(){
    using namespace openp1::access;
    assert(default_policy==2);
    assert(migrate_default(1,false)==2);
    assert(migrate_default(0,false)==0);
    assert(migrate_default(1,true)==1);
    assert(migrate_default(2,true)==2);
    assert(migrate_default(255,false)==255);
    Grant g;
    for(std::uint8_t p=0;p<3;++p){
        assert(g.effective(p,10)==p);
        assert(g.permits(p,false,1,10)==(p>=1));assert(g.permits(p,false,2,10)==(p==2));
        assert(!g.permits(p,true,1,10));
    }
    g.activate(1000);assert(g.remaining(1000)==300);assert(g.remaining(300999)==1);
    assert(g.effective(0,300999)==2);assert(g.effective(0,301000)==0);
    assert(g.effective(1,301000)==1);assert(g.effective(255,2000)==0);
    assert(!g.permits(0,true,2,1001));
    Grant reboot;assert(reboot.effective(0,1001)==0);
    Button quick;unsigned grants=0;
    for(unsigned t=0;t<=250;t+=poll_ms)if(quick.update(t>=10&&t<110,t)==Action::Grant)++grants;
    assert(grants==1); // An ordinary 100 ms tap must not be missed by integration polling.
    Button b;assert(b.update(false,0)==Action::None);
    b.update(true,100);b.update(false,120);assert(b.update(false,200)==Action::None); // bounce
    b.update(true,300);b.update(true,350);b.update(false,500);
    assert(b.update(false,550)==Action::Grant);assert(b.update(false,900)==Action::None);
    b.update(true,1000);b.update(true,1050);assert(b.countdown(2050)==7);
    assert(b.update(true,9049)==Action::None);assert(b.update(true,9050)==Action::ResetWifi);
    assert(b.update(true,10000)==Action::None);b.update(false,11000);
    assert(b.update(false,11050)==Action::None); // long release never grants
    Button boot;boot.update(true,0);assert(boot.update(true,9000)==Action::None);
    boot.update(false,10000);assert(boot.update(false,10050)==Action::None);
    const std::string deep(9,'[');assert(!openp1::http::shallow_json((deep+std::string(9,']')).c_str(),18));
    assert(openp1::http::shallow_json("{\"x\":\"[[]]\"}",12));
}
