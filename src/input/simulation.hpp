#pragma once
#include "pipeline.hpp"
#include <cstring>
namespace openp1::input {
// Original reassembled capture; only same-length meter identifiers are replaced.
inline constexpr char default_simulation_text[]=
    "E6E7000F009D41010C07EA0A1503161E1900FF88800210020209060000600100FF090831323334353637380202090600"
    "00600101FF090731323334353637020309060100010700FF06000003B602020F00161B020309060100020700FF060000"
    "000002020F00161B020309060100010800FF06008D9FBF02020F00161E020309060100020800FF060059AABC02020F00"
    "161E020309060100200700FF12097102020FFF1623020309060100340700FF12095F02020FFF16230203090601004807"
    "00FF12097A02020FFF16230203090601001F0700FF12001E02020FFE1621020309060100330700FF12012C02020FFE16"
    "21020309060100470700FF12008302020FFE1621020309060100010801FF06001776C202020F00161E02030906010001"
    "0802FF06007628FD02020F00161E020309060100020801FF0600025B3E02020F00161E020309060100020802FF060057"
    "4F7E02020F00161E";
struct SimulationConfig {
    static constexpr std::size_t max_text=4096;
    std::uint32_t version=1;
    std::uint8_t enabled=0,layer=1;
    char text[max_text+1]{};
    SimulationConfig() noexcept {reset();}
    void reset() noexcept {
        version=1;enabled=0;layer=1;std::memset(text,0,sizeof(text));
        std::memcpy(text,default_simulation_text,sizeof(default_simulation_text));
    }
};
struct SimulationData {
    bool enabled=false;
    Layer layer=Layer::LlcApdu;
    std::array<std::uint8_t,2048> bytes{};
    std::size_t size=0;
};
// Bounded syntax validation; protocol failures are left intact for the shared pipeline.
bool prepare_simulation(const SimulationConfig& config,SimulationData& out,char* error,std::size_t capacity);
class SimulationPlayer {
public:
    explicit SimulationPlayer(const SimulationData& data):data_(data){}
    // Acquisition owner only. Returns true when a cycle finishes.
    bool tick(Pipeline& pipeline,std::uint64_t now);
private:
    const SimulationData& data_;
    std::size_t position_=0;
    std::uint64_t next_at_=0;
};
}
