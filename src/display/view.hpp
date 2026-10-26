#pragma once
#include "meter/protocol.hpp"
#include <array>
#include <cstdint>
namespace openp1::display {
enum class MqttState : std::uint8_t {Disabled,Waiting,Connected,Error};
struct View {
    std::array<std::array<char,21>,8> lines{}; // Footer/status retained for provisioning and S2.
    std::array<char,12> power{"--"};
    std::array<char,4> unit{"W"};
    std::array<char,12> flow{"Waiting"};
    std::array<std::array<char,12>,3> voltage{};
    std::array<bool,3> voltage_stale{};
    bool power_valid=false,power_stale=false,exporting=false,simulation=false;
    bool wifi_connected=false,receiving=false;
    std::uint8_t wifi_bars=0;
    MqttState mqtt=MqttState::Disabled;
};
static_assert(sizeof(View)<=256,"Keep the display mailbox bounded");
struct ReceptionPulse {
    std::uint64_t count=0,until=0;
    void update(std::uint64_t next,std::uint64_t now){if(next>count)until=now+400;else if(next<count)until=0;count=next;}
    bool active(std::uint64_t now)const{return now<until;}
};
View make_view(const meter::Processor& processor, bool simulation, std::uint64_t now);
// Caller provides 240*135 RGB565 words in big-endian wire order. Bounds are fixed.
void render(const View& view, std::uint16_t* pixels);
// Render a bounded output-row band; rotation maps the complete 240x135 image.
void render_rows(const View& view,std::uint16_t* pixels,std::size_t first,std::size_t rows,bool rotate);
inline constexpr std::size_t transfer_rows=8;
bool rotated();
void set_rotated(bool enabled);
bool start(); // Own task, latest-view mailbox, no acquisition-side SPI waits.
void submit(const View& view);
}
