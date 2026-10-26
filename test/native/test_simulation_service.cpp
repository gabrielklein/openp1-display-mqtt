#include "simulation/service.hpp"
#include "nvs.h"
#include <cassert>
std::int64_t fake_time_us=1000000;
int main(int argc,char** argv){
    using namespace openp1;
    const bool saved=argc>1&&std::strcmp(argv[1],"saved")==0;
    if(argc>1){
        input::SimulationConfig c;c.enabled=saved?1:3;
        nvs_set_blob(1,"simulation",&c,sizeof(c));assert(nvs_commit(1)==ESP_OK);
    }
    assert(simulation::start());
    assert(simulation::boot_data().enabled==saved);
    assert(!simulation::restart_due(1000000));
    input::SimulationConfig c;assert(simulation::get(c)&&c.enabled==saved);
    char error[128]{};c.enabled=!saved;
    fake_commit_ok=false;assert(!simulation::configure(c,error,sizeof(error)));fake_commit_ok=true;
    assert(!simulation::restart_due(1000000));
    input::SimulationConfig unchanged;assert(simulation::get(unchanged)&&unchanged.enabled==saved);
    std::strcpy(c.text,"invalid");assert(!simulation::configure(c,error,sizeof(error))&&error[0]);
    assert(!simulation::restart_due(1000000));
    std::strcpy(c.text,input::default_simulation_text);
    fake_time_us=((std::int64_t{1}<<32)-500)*1000;
    assert(simulation::configure(c,error,sizeof(error)));
    assert(!simulation::restart_due(1000000));simulation::request_restart();
    const auto now=static_cast<std::uint64_t>(fake_time_us/1000);
    assert(!simulation::restart_due(now+1499)&&simulation::restart_due(now+1500));
    assert(simulation::get(unchanged)&&unchanged.enabled!=saved);
    assert(simulation::boot_data().enabled==saved);
    input::SimulationConfig persisted;std::size_t size=sizeof(persisted);
    assert(nvs_get_blob(1,"simulation",&persisted,&size)==ESP_OK&&persisted.enabled==c.enabled);
}
