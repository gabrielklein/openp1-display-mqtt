#pragma once
#include <cstdint>
namespace openp1::access {
inline constexpr unsigned poll_ms=10;
inline constexpr std::uint8_t default_policy=2;
// One-time migration of the old View default; never reopen a saved None.
inline std::uint8_t migrate_default(std::uint8_t saved,bool applied){return !applied&&saved==1?default_policy:saved;}
// Values match persisted M3 policy. Unknown values always fail closed.
inline bool valid(std::uint8_t p) { return p<=2; }
inline const char* name(std::uint8_t p) { return p==2?"Full":p==1?"View":"None"; }
struct Grant {
    std::uint64_t until=0;
    void activate(std::uint64_t now) { until=now+300000; }
    std::uint32_t remaining(std::uint64_t now) const {
        return until>now?static_cast<std::uint32_t>((until-now+999)/1000):0;
    }
    std::uint8_t effective(std::uint8_t persisted,std::uint64_t now) const {
        return valid(persisted)?(until>now?2:persisted):0;
    }
    bool permits(std::uint8_t persisted,bool setup,std::uint8_t required,std::uint64_t now) const {
        return !setup && required>=1 && required<=2 && effective(persisted,now)>=required;
    }
};
enum class Action { None, Grant, ResetWifi };
class Button {
public:
    Action update(bool pressed,std::uint64_t now) {
        if(!initialized_) { initialized_=true;raw_=stable_=pressed;changed_=now;armed_=!pressed;return Action::None; }
        if(pressed!=raw_) { raw_=pressed;changed_=now; }
        if(raw_!=stable_ && now-changed_>=50) {
            stable_=raw_;
            if(stable_) { since_=now;fired_=false; }
            else {
                const bool short_press=armed_&&!fired_;
                armed_=true;
                if(short_press) return Action::Grant;
            }
        }
        if(stable_ && armed_ && !fired_ && now-since_>=8000) {
            fired_=true;return Action::ResetWifi;
        }
        return Action::None;
    }
    bool pressed() const { return stable_; }
    unsigned countdown(std::uint64_t now) const {
        if(!stable_||!armed_||fired_||now-since_<1000) return 0;
        const auto held=now-since_;return held>=8000?0:static_cast<unsigned>((8000-held+999)/1000);
    }
private:
    bool initialized_=false,raw_=false,stable_=false,armed_=false,fired_=false;
    std::uint64_t changed_=0,since_=0;
};
}
