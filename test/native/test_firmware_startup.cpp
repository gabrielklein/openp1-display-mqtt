#include "firmware/update.hpp"
#include <cassert>
std::int64_t fake_time_us=0;
int main(int argc,char**){
    fake_running=&fake_ota0;fake_ota_state=ESP_OTA_IMG_PENDING_VERIFY;
    openp1::firmware::startup_tick(29999,true);assert(fake_ota_confirms==0&&fake_ota_rollbacks==0);
    if(argc>1){openp1::firmware::startup_tick(30000,false);assert(fake_ota_rollbacks==1&&fake_ota_confirms==0);}
    else{
        fake_ota_state_ok=false;openp1::firmware::startup_tick(30000,true);assert(fake_ota_confirms==0);
        fake_ota_state_ok=true;openp1::firmware::startup_tick(30001,true);assert(fake_ota_confirms==1);
        openp1::firmware::startup_tick(31000,true);assert(fake_ota_confirms==1);
    }
}
