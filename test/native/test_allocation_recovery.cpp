#include "input/pipeline.hpp"
#include "meter/hex.hpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <new>
static long failure_after=-1;
void* operator new(std::size_t size){
    if(failure_after==0){failure_after=-1;throw std::bad_alloc();}
    if(failure_after>0)--failure_after;
    if(void* p=std::malloc(size?size:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
int main(){
    using namespace openp1;
    std::ifstream f("test/fixtures/isk-1-llc.txt");const std::string text(std::istreambuf_iterator<char>(f),{});
    meter::Bytes bytes;assert(meter::parse_hex(text,bytes));
    unsigned recovered=0,successes=0;
    for(long fail=0;fail<160;++fail){
        input::Pipeline pipeline;pipeline.select(input::Source::Replay);
        auto cycle=[&](std::uint64_t now){
            for(std::size_t i=0;i<bytes.size();i+=64)assert(pipeline.ingest(input::Source::Replay,input::Layer::LlcApdu,bytes.data()+i,std::min<std::size_t>(64,bytes.size()-i),now));
            pipeline.finish(now);
        };
        cycle(100);const auto expected=pipeline.processor().readings();assert(expected.size()==16);
        failure_after=fail;cycle(200);failure_after=-1;
        const auto& processor=pipeline.processor();
        if(processor.counters().last_error==meter::Error::Memory){
            ++recovered;assert(processor.counters().notifications==1);
            assert(processor.readings().size()==expected.size());
            for(std::size_t i=0;i<expected.size();++i){assert(processor.readings()[i].encoded_value==expected[i].encoded_value);assert(processor.readings()[i].reading.t_ms==100);}
            assert(processor.last_document().encoded==bytes);
        }else{++successes;assert(processor.counters().notifications==2);}
        // Failed decoding still retains the exact source chunks.
        const auto& last=pipeline.at(pipeline.size()-1);assert(last.t_ms==200&&last.size>0);
        assert(std::equal(last.bytes.begin(),last.bytes.begin()+last.size,bytes.end()-last.size));
        const auto before=processor.counters().notifications;cycle(300);
        assert(processor.counters().notifications==before+1);
        for(const auto& reading:processor.readings())assert(reading.reading.t_ms==300);
    }
    assert(recovered>20&&successes>0);
}
