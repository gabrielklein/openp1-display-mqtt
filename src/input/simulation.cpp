#include "simulation.hpp"
#include <algorithm>
#include <cstdio>
namespace openp1::input {
bool prepare_simulation(const SimulationConfig& config,SimulationData& out,char* error,std::size_t capacity){
    out.size=0;out.enabled=false;
    auto fail=[&](const char* message){if(capacity)std::snprintf(error,capacity,"%s",message);return false;};
    if(capacity)error[0]=0;
    if(config.version!=1||config.enabled>1||config.layer>1||!std::memchr(config.text,0,sizeof(config.text)))
        return fail("Invalid simulation settings");
    std::string_view text(config.text);std::size_t size=0;
    while(!text.empty()){
        const auto end=text.find('\n');auto line=text.substr(0,end);
        text=end==text.npos?std::string_view{}:text.substr(end+1);
        const auto first=line.find_first_not_of(" \t\r");if(first==line.npos)continue;
        line.remove_prefix(first);
        const auto space=line.find_first_of(" \t");
        if(line.substr(0,space).find('/')!=line.npos){
            if(space==line.npos)return fail("Missing hex payload after topic");
            line.remove_prefix(space);
        }
        const auto line_start=size;int high=-1;
        for(char c:line){
            if(c==' '||c=='\t'||c=='\r')continue;
            const int nibble=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
            if(nibble<0)return fail("Invalid hex sequence; use hex bytes or topic/with/slash hex lines");
            if(high<0)high=nibble;
            else{
                if(size==out.bytes.size())return fail("Simulation exceeds 2048 raw bytes");
                out.bytes[size++]=static_cast<std::uint8_t>((high<<4)|nibble);high=-1;
            }
        }
        if(high>=0)return fail("Incomplete hex byte at end of line");
        if(size==line_start)return fail("Missing hex bytes on line");
    }
    if(!size)return fail("Simulation sequence is empty");
    out.size=size;out.layer=config.layer?Layer::LlcApdu:Layer::WireHdlc;out.enabled=config.enabled==1;return true;
}
bool SimulationPlayer::tick(Pipeline& pipeline,std::uint64_t now){
    if(!data_.enabled||!data_.size||data_.size>data_.bytes.size()||pipeline.source()!=Source::Replay||now<next_at_)return false;
    if(!position_)pipeline.finish(now);
    const auto count=std::min<std::size_t>(64,data_.size-position_);
    if(!pipeline.ingest(Source::Replay,data_.layer,data_.bytes.data()+position_,count,now))return false;
    position_+=count;
    if(position_==data_.size){
        pipeline.finish(now);position_=0;next_at_=now+3500;return true;
    }
    next_at_=now+20;return false;
}
}
