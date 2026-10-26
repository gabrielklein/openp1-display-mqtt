#include "mqtt/text.hpp"
#include "input/pipeline.hpp"
#include "meter/hex.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
std::string file(const char* name){std::ifstream f(name);assert(f);return std::string(std::istreambuf_iterator<char>(f),{});}
int main(){
    using namespace openp1;
    meter::Bytes bytes;assert(meter::parse_hex(file("test/fixtures/isk-text-llc.txt"),bytes));
    meter::Processor p;p.feed(bytes.data(),bytes.size(),false,100);p.finish(100);
    assert(p.counters().notifications==1);
    meter::PresentationSnapshot s;s.capture(p,false);char out[192];
    for(const auto& pair:{std::pair<const char*,const char*>{"active","464 0 9393.88 5882.48"},{"voltage","240.57 240.70 239.50 241.50"},{"current","2.69 0.54 1.63 0.52"},{"metering","1558133 7835747 154430 5728054"}}){
        assert(mqtt::text_summary(s,1000,pair.first,out,sizeof(out))&&std::string(out)==pair.second);
        assert(!mqtt::text_summary(s,15100,pair.first,out,sizeof(out)));
        assert(!mqtt::text_summary(s,1000,pair.first,out,2));
    }
    for(std::size_t i=0;i<s.count;++i){
        if(s.values[i].obis==std::array<std::uint8_t,6>{1,0,51,7,0,255}){
            assert(mqtt::obis_payload(s,i,1000,out,sizeof(out))&&std::string(out)=="1.63");
            assert(!mqtt::obis_payload(s,i,15100,out,sizeof(out)));
            assert(!mqtt::obis_payload(s,i,0,out,sizeof(out)));
            assert(!mqtt::obis_payload(s,i,1000,out,2));
            auto invalid=s;invalid.values[i].scaled=std::numeric_limits<double>::infinity();assert(!mqtt::obis_payload(invalid,i,1000,out,sizeof(out)));
        }
    }
    assert(mqtt::obis_payload(s,0,1000,out,sizeof(out))&&std::string(out)=="12345678");
    assert(!mqtt::obis_payload(s,s.count,1000,out,sizeof(out)));
    auto old=s;
    for(std::size_t i=0;i<s.count;++i)if(s.values[i].obis[2]==32){s.values[i].t_ms=50;break;}
    assert(!mqtt::text_summary(s,1000,"voltage",out,sizeof(out)));s=old;
    for(std::size_t i=0;i<s.count;++i)if(s.values[i].obis[2]==31){s.values[i].unit=35;break;}
    assert(!mqtt::text_summary(s,1000,"current",out,sizeof(out)));s=old;
    s.count=0;assert(!mqtt::text_summary(s,1000,"active",out,sizeof(out)));s=old;
    s.values[2].scaled=std::numeric_limits<double>::infinity();assert(!mqtt::text_summary(s,1000,"active",out,sizeof(out)));
    // Observe original frame bodies with embedded flags and the exact reassembly.
    const auto text=file("test/fixtures/isk-1-wire.txt");input::Replay replay;std::string error;
    assert(replay.load(text,input::Layer::WireHdlc,error));input::Pipeline pipeline;pipeline.select(input::Source::Replay);
    std::vector<meter::Bytes> frames,packets;
    pipeline.observe([&](const std::uint8_t* data,std::size_t size,std::uint64_t){meter::Bytes frame{0x7e};frame.insert(frame.end(),data,data+size);frame.push_back(0x7e);frames.push_back(frame);},
        [&](const meter::Bytes& data,std::uint64_t){packets.push_back(data);});
    while(!replay.done())assert(replay.next(pipeline,1,100));
    assert(frames.size()==3&&packets.size()==1);
    std::size_t pos=0;
    for(const auto& frame:frames){const auto end=text.find('\n',pos),space=text.find(' ',pos);meter::Bytes expected;assert(meter::parse_hex(text.substr(space+1,end-space-1),expected));assert(frame==expected);pos=end+1;}
    assert(meter::parse_hex(file("test/fixtures/isk-1-llc.txt"),bytes)&&packets[0]==bytes);
    auto invalid=frames[0];invalid[invalid.size()-2]^=1;
    pipeline.ingest(input::Source::Replay,input::Layer::WireHdlc,invalid.data(),invalid.size(),200);
    assert(frames.size()==4&&frames.back()==invalid&&packets.size()==1); // Bad checksum stays raw.
    pipeline.select(input::Source::Uart);pipeline.select(input::Source::Replay);frames.clear();packets.clear();
    for(std::size_t i=0;i<bytes.size();i+=64)pipeline.ingest(input::Source::Replay,input::Layer::LlcApdu,bytes.data()+i,std::min<std::size_t>(64,bytes.size()-i),300);
    pipeline.finish(300);assert(frames.empty()&&packets.size()==1&&packets[0]==bytes);
}
