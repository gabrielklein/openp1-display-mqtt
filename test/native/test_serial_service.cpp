#include "config/serial.hpp"
#include "nvs.h"
#include <cassert>
std::int64_t fake_time_us=0;
int main(){
    using namespace openp1;
    input::SerialSettings defaults;assert(serial::start(defaults));
    input::SerialSettings saved;assert(serial::get(saved)&&saved.baud==115200);
    auto changed=defaults;changed.baud=19200;changed.parity=input::Parity::Even;changed.stop_bits=input::StopBits::Two;changed.inverted=false;
    assert(serial::configure(changed));assert(serial::get(saved)&&saved.baud==19200&&!saved.inverted);
    assert(serial::boot_settings().baud==115200); // Immutable until next boot.
    defaults.baud=1200;assert(serial::start(defaults));
    assert(serial::boot_settings().baud==19200&&serial::boot_settings().parity==input::Parity::Even&&serial::boot_settings().stop_bits==input::StopBits::Two);
    fake_commit_ok=false;changed.baud=38400;assert(!serial::configure(changed));assert(serial::get(saved)&&saved.baud==19200);fake_commit_ok=true;
    changed.baud=0;assert(!serial::configure(changed));
    const auto original=fake_nvs["serial"];
    for(std::size_t size:{std::size_t(1),std::size_t(11),std::size_t(13)}){
        fake_nvs["serial"].assign(size,0);assert(!serial::start(defaults));assert(serial::boot_settings().baud==1200);
        assert(serial::get(saved)&&saved.baud==1200); // Corrupt storage remains repairable.
    }
    fake_nvs["serial"]=original;fake_nvs["serial"][0]=99;assert(!serial::start(defaults));
    fake_nvs["serial"]=original;fake_nvs["serial"][8]=255;assert(!serial::start(defaults));
    fake_nvs["serial"]=original;fake_nvs["serial"][9]=0;assert(!serial::start(defaults));
    fake_nvs["serial"]=original;fake_nvs["serial"][10]=2;assert(!serial::start(defaults));
    changed=defaults;assert(serial::configure(changed));assert(serial::start(defaults));assert(serial::boot_settings().baud==1200);
}
