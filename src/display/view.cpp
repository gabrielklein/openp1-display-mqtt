#include "view.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace openp1::display {
View make_view(const meter::Processor& processor, bool simulation, std::uint64_t now) {
    View view;view.simulation=simulation;
    for(auto& voltage:view.voltage)std::snprintf(voltage.data(),voltage.size(),"-- V");
    auto line = [&](std::size_t i) { return view.lines[i].data(); };
    std::snprintf(line(0), 21, "%s", simulation ? "Simulation" : "OpenP1 UART");
    const auto& samples = processor.readings();
    std::uint64_t latest = 0;
    bool any = false;
    for (const auto& s : samples) if (s.reading.numeric) { latest = std::max(latest, s.reading.t_ms); any = true; }
    if (!any) {
        std::snprintf(line(1), 21, "No readings err %llu",
                      static_cast<unsigned long long>(std::min<std::uint64_t>(processor.counters().errors, 9999)));
        std::snprintf(line(3), 21, "%s", meter::error_name(processor.counters().last_error));
        return view;
    }
    const auto age = now >= latest ? now - latest : 0;
    std::snprintf(line(1), 21, "%s %llus err %llu", age >= 15000 ? "Stale" : "Age",
                  static_cast<unsigned long long>(std::min<std::uint64_t>(age / 1000, 9999)),
                  static_cast<unsigned long long>(std::min<std::uint64_t>(processor.counters().errors, 9999)));
    view.simulation=simulation;
    auto reading=[&](unsigned code,unsigned unit)->const meter::Reading*{
        for(const auto& sample:samples){const auto& r=sample.reading;
            if(r.obis==std::array<std::uint8_t,6>{1,0,static_cast<std::uint8_t>(code),7,0,255}&&r.attribute==2&&r.numeric&&r.has_scaler_unit&&r.unit==unit&&std::isfinite(r.scaled)&&r.scaled>=0)return &r;
        }return nullptr;
    };
    const auto* imported=reading(1,27);const auto* exported=reading(2,27);
    bool export_present=false;for(const auto& sample:samples)if(sample.reading.obis==std::array<std::uint8_t,6>{1,0,2,7,0,255})export_present=true;
    if(imported&&(!export_present||(exported&&exported->t_ms==imported->t_ms))){
        const auto net=imported->scaled-(exported?exported->scaled:0.0),magnitude=std::abs(net);
        view.power_valid=true;view.power_stale=now<imported->t_ms||now-imported->t_ms>=15000;view.exporting=net<0;
        std::snprintf(view.flow.data(),view.flow.size(),"%s",net<0?"Export":net>0?"Import":"Balanced");
        if(magnitude<1000)std::snprintf(view.power.data(),view.power.size(),"%.0f",magnitude);
        else{std::snprintf(view.unit.data(),view.unit.size(),"kW");std::snprintf(view.power.data(),view.power.size(),magnitude<10000000?"%.2f":"%.3g",magnitude/1000);}
    }
    for(std::size_t phase=0;phase<3;++phase){
        const auto* r=reading(32+phase*20,35);
        if(r){std::snprintf(view.voltage[phase].data(),view.voltage[phase].size(),r->scaled<10000?"%.1fV":"%.2gV",r->scaled);view.voltage_stale[phase]=now<r->t_ms||now-r->t_ms>=15000;}
        else std::snprintf(view.voltage[phase].data(),view.voltage[phase].size(),"-- V");
    }
    return view;
}
namespace {
// Original 5x7 bitmap glyphs, each row five bits; no external font dependency.
using Glyph = std::array<std::uint8_t, 7>;
Glyph glyph(char c) {

    switch (c) {
    case 'a': return {0,0,14,1,15,17,15}; case 'b': return {16,16,30,17,17,17,30};
    case 'c': return {0,0,14,16,16,17,14}; case 'd': return {1,1,15,17,17,17,15};
    case 'e': return {0,0,14,17,31,16,14}; case 'f': return {6,9,8,28,8,8,8};
    case 'g': return {0,15,17,17,15,1,14}; case 'h': return {16,16,30,17,17,17,17};
    case 'i': return {4,0,12,4,4,4,14}; case 'j': return {2,0,6,2,2,18,12};
    case 'k': return {16,16,18,20,24,20,18}; case 'l': return {12,4,4,4,4,4,14};
    case 'm': return {0,0,26,21,21,21,21}; case 'n': return {0,0,30,17,17,17,17};
    case 'o': return {0,0,14,17,17,17,14}; case 'p': return {0,30,17,17,30,16,16};
    case 'q': return {0,15,17,17,15,1,1}; case 'r': return {0,0,22,25,16,16,16};
    case 's': return {0,0,15,16,14,1,30}; case 't': return {8,8,28,8,8,9,6};
    case 'u': return {0,0,17,17,17,19,13}; case 'v': return {0,0,17,17,17,10,4};
    case 'w': return {0,0,17,17,21,21,10}; case 'x': return {0,0,17,10,4,10,17};
    case 'y': return {0,17,17,17,15,1,14}; case 'z': return {0,0,31,2,4,8,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,14}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,2,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,25,21,19,19,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '.': return {0,0,0,0,0,12,12}; case ':': return {0,12,12,0,12,12,0};
    case '-': return {0,0,0,31,0,0,0}; case '+': return {0,4,4,31,4,4,0};
    case '*': return {0,21,14,31,14,21,0}; case '/': return {1,1,2,4,8,16,16};
    case ' ': return {}; default: return {14,17,1,2,4,0,4};
    }
}
}
namespace {
constexpr std::uint16_t rgb(unsigned r,unsigned g,unsigned b){const auto value=((r&248)<<8)|((g&252)<<3)|(b>>3);return static_cast<std::uint16_t>((value<<8)|(value>>8));}
constexpr auto background=rgb(9,19,25),card=rgb(16,32,39),muted=rgb(118,143,153),green=rgb(63,221,153),red=rgb(255,100,101),amber=rgb(255,194,82),white=std::uint16_t{0xffff};
struct Painter {
    std::uint16_t* pixels;std::size_t first,rows;bool rotate;
    void pixel(int x,int y,std::uint16_t color){if(x<0||x>=240||y<0||y>=135)return;const auto dx=rotate?239-x:x,dy=rotate?134-y:y;if(static_cast<std::size_t>(dy)>=first&&static_cast<std::size_t>(dy)<first+rows)pixels[(dy-first)*240+dx]=color;}
    void rect(int x,int y,int w,int h,std::uint16_t color){for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)pixel(xx,yy,color);}
    void text(int x,int y,const char* str,int scale,std::uint16_t color){for(std::size_t i=0;i<40&&str[i];++i){const auto g=glyph(str[i]);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(g[row]&(1u<<(4-col)))rect(x+static_cast<int>(i)*6*scale+col*scale,y+row*scale,scale,scale,color);}}
};
}
void render_rows(const View& view,std::uint16_t* pixels,std::size_t first,std::size_t rows,bool rotate){
    if(!pixels||first>=135||rows>135-first)return;
    std::fill_n(pixels,240*rows,background);Painter p{pixels,first,rows,rotate};
    const auto accent=!view.power_valid?muted:view.power_stale?amber:view.flow[0]=='B'?muted:view.exporting?green:red;
    p.text(9,7,view.simulation?"Simulation":"OpenP1",1,view.simulation?amber:muted);
    // Radio arcs, with dim outer arcs for lower signal strength.
    for(int y=-8;y<=0;++y)for(int x=-8;x<=8;++x){const auto d=x*x+y*y;
        for(int ring=1;ring<=3;++ring){const int radius=ring*2+1;if(d>=radius*radius-2&&d<=radius*radius+2)p.pixel(134+x,14+y,view.wifi_connected&&view.wifi_bars>=ring?green:muted);}}
    p.rect(133,14,3,2,view.wifi_connected?green:red);
    if(!view.wifi_connected){for(int i=0;i<9;++i){p.pixel(130+i,6+i,red);p.pixel(138-i,6+i,red);}}
    const auto mqtt_color=view.mqtt==MqttState::Connected?green:view.mqtt==MqttState::Error?red:view.mqtt==MqttState::Waiting?amber:muted;
    p.rect(154,3,19,15,mqtt_color);p.rect(155,4,17,13,background);p.text(158,7,"MQ",1,mqtt_color);
    p.text(186,7,"RX",1,muted);p.rect(206,6,9,9,muted);p.rect(207,7,7,7,view.receiving?green:background);
    p.rect(6,23,228,65,card);p.rect(6,26,3,59,accent);
    p.text(16,29,view.flow.data(),1,accent);
    if(view.power_stale)p.text(192,29,"Stale",1,amber);
    const auto length=std::strlen(view.power.data());const int scale=length<=5?5:length<=7?4:3;
    const int unit_width=static_cast<int>(std::strlen(view.unit.data()))*12;
    const int total=static_cast<int>(length)*6*scale+unit_width+8;
    const int x=std::max(12,(240-total)/2);
    p.text(x,46,view.power.data(),scale,view.power_stale?amber:white);
    p.text(x+static_cast<int>(length)*6*scale+8,46+7*scale-14,view.unit.data(),2,accent);
    p.rect(10,94,220,1,muted);
    for(std::size_t phase=0;phase<3;++phase){const int xphase=12+static_cast<int>(phase)*78;char label[3]={'L',static_cast<char>('1'+phase),0};
        p.text(xphase,100,label,1,muted);p.text(xphase,109,view.voltage[phase].data(),std::strlen(view.voltage[phase].data())<=6?2:1,view.voltage_stale[phase]?amber:white);}
    p.text(8,127,view.lines[6].data(),1,muted);
    p.text(232-static_cast<int>(std::strlen(view.lines[7].data()))*6,127,view.lines[7].data(),1,muted);
}
void render(const View& view,std::uint16_t* pixels){render_rows(view,pixels,0,135,false);}
}
