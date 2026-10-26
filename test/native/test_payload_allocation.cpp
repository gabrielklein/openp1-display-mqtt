#include "mqtt/payload.hpp"
#include <cassert>
#include <cstdlib>
#include <new>
static long fail_c=-1,fail_cpp=-1;
static long live=0;
void* operator new(std::size_t n){if(fail_cpp==0){fail_cpp=-1;throw std::bad_alloc();}if(fail_cpp>0)--fail_cpp;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* allocate(std::size_t size){if(fail_c==0){fail_c=-1;return nullptr;}if(fail_c>0)--fail_c;void* p=std::malloc(size);if(p)++live;return p;}
void release(void* p){if(p)--live;std::free(p);}
int main(){
    using namespace openp1;
    meter::PresentationSnapshot s;s.count=1;s.values[0].numeric=true;s.values[0].obis={1,0,1,7,0,255};s.values[0].t_ms=100;
    cJSON_Hooks hooks{allocate,release};cJSON_InitHooks(&hooks);
    for(long i=0;i<120;++i){
        fail_c=i;auto* j=mqtt::state_json(s,1000);fail_c=-1;
        if(j){assert(cJSON_GetObjectItemCaseSensitive(j,"values"));cJSON_Delete(j);}assert(live==0);
        fail_c=i;j=mqtt::discovery_json(s.values[0],"custom/normal","device");fail_c=-1;
        if(j){assert(cJSON_GetObjectItemCaseSensitive(j,"device"));cJSON_Delete(j);}assert(live==0);
    }
    for(long i=0;i<30;++i){
        fail_cpp=i;try{auto* j=mqtt::discovery_json(s.values[0],"custom/normal","device");cJSON_Delete(j);}catch(const std::bad_alloc&){}
        fail_cpp=-1;assert(live==0);
    }
    // Long meter text requires std::string allocation; failure returns no partial payload.
    s.values[0].numeric=false;s.values[0].type=9;s.encoded_size=203;s.lengths[0]=203;
    s.encoded[0]=9;s.encoded[1]=0x81;s.encoded[2]=200;std::fill_n(s.encoded.begin()+3,200,'x');
    fail_cpp=0;assert(mqtt::state_json(s,1000)==nullptr);fail_cpp=-1;assert(live==0);
    auto* j=mqtt::state_json(s,1000);assert(j);cJSON_Delete(j);assert(live==0);
    cJSON_InitHooks(nullptr);
}
