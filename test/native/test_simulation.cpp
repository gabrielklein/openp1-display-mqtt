#include "input/simulation.hpp"
#include "meter/presentation.hpp"
#include "meter/hex.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
int main(){
    using namespace openp1::input;
    SimulationConfig c;SimulationData data;char error[128];
    assert(c.enabled==0&&prepare_simulation(c,data,error,sizeof(error))&&!data.enabled);
    c.enabled=1;assert(prepare_simulation(c,data,error,sizeof(error)));
    Pipeline pipeline;pipeline.select(Source::Replay);SimulationPlayer player(data);
    std::uint64_t now=0;
    while(!player.tick(pipeline,now))now+=20;
    assert(pipeline.processor().counters().notifications==1&&pipeline.processor().counters().validated_hdlc==0);
    const auto first_size=pipeline.size();
    assert(!player.tick(pipeline,now+3499)&&pipeline.size()==first_size);
    player.tick(pipeline,now+3500);assert(pipeline.size()==first_size+1);
    auto last=now+3500;
    while(pipeline.processor().counters().notifications<2){last+=20;player.tick(pipeline,last);}
    assert(pipeline.processor().readings().size()==16);
    const auto& readings=pipeline.processor().readings();
    std::string text;bool binary=false;
    assert(openp1::meter::string_value(readings[0].encoded_value.data(),readings[0].encoded_value.size(),text,binary)&&text=="12345678");
    assert(openp1::meter::string_value(readings[1].encoded_value.data(),readings[1].encoded_value.size(),text,binary)&&text=="1234567");
    for(const auto& r:readings)assert(r.reading.t_ms==last);
    // The anonymized fixture matches the built-in simulation byte for byte.
    std::ifstream file("test/fixtures/isk-1-llc.txt");std::string capture(std::istreambuf_iterator<char>(file),{});
    openp1::meter::Bytes anonymized;assert(openp1::meter::parse_hex(capture,anonymized));
    assert(anonymized.size()==data.size&&std::equal(anonymized.begin(),anonymized.end(),data.bytes.begin()));
    Replay original;std::string load_error;assert(original.load(capture,Layer::LlcApdu,load_error));
    Pipeline reference;reference.select(Source::Replay);while(!original.done())assert(original.next(reference,64,0));
    assert(reference.processor().readings().size()==readings.size());
    for(std::size_t i=2;i<readings.size();++i)assert(readings[i].encoded_value==reference.processor().readings()[i].encoded_value);
    for(const char* invalid:{"","7","zz","7e\nzz","topic/raw","7e\ntopic/raw ","{\"hex\":\"7e\"}"}){
        std::strcpy(c.text,invalid);assert(!prepare_simulation(c,data,error,sizeof(error))&&error[0]&&!data.enabled&&data.size==0);
    }
    std::strcpy(c.text,"7e a8 7e\ntopic/raw 00ff7e01");c.layer=0;
    assert(prepare_simulation(c,data,error,sizeof(error))&&data.size==7&&data.layer==Layer::WireHdlc);
    Pipeline bad;bad.select(Source::Replay);SimulationPlayer bad_player(data);assert(bad_player.tick(bad,0));
    assert(bad.at(0).size==7&&bad.at(0).bytes[1]==0xa8&&bad.processor().counters().validated_hdlc==0);
    Pipeline uart;SimulationPlayer guarded(data);assert(!guarded.tick(uart,0)&&uart.size()==0);
    std::memset(c.text,'a',sizeof(c.text));assert(!prepare_simulation(c,data,error,sizeof(error)));
}
