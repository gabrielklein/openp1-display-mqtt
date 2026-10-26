#include "input/pipeline.hpp"
#include "meter/hex.hpp"
#include "display/view.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

using namespace openp1;
using meter::Bytes;
std::string file(const std::string& path) {
    std::ifstream stream(path);
    assert(stream.good());
    return std::string(std::istreambuf_iterator<char>(stream), {});
}
Bytes hex(const std::string& text) {
    Bytes result;
    assert(meter::parse_hex(text, result));
    return result;
}
Bytes wire(unsigned index) {
    const auto text = file("test/fixtures/isk-" + std::to_string(index) + "-wire.txt");
    Bytes output;
    std::size_t pos = 0;
    while (pos < text.size()) {
        auto end = text.find('\n', pos);
        if (end == text.npos) end = text.size();
        const auto space = text.find(' ', pos);
        const auto part = hex(text.substr(space + 1, end - space - 1));
        output.insert(output.end(), part.begin(), part.end());
        pos = end + 1;
    }
    return output;
}
void feed(meter::Processor& processor, const Bytes& bytes, std::size_t step, std::uint64_t now = 100) {
    for (std::size_t pos = 0; pos < bytes.size(); pos += step)
        processor.feed(bytes.data() + pos, std::min(step, bytes.size() - pos), true, now);
}
const meter::Sample& sample(const meter::Processor& p, unsigned c, unsigned d, unsigned e = 0) {
    const auto it = std::find_if(p.readings().begin(), p.readings().end(), [&](const auto& s) {
        return s.reading.obis == std::array<std::uint8_t,6>{1,0,static_cast<std::uint8_t>(c),
                  static_cast<std::uint8_t>(d),static_cast<std::uint8_t>(e),255};
    });
    assert(it != p.readings().end());
    return *it;
}
void equal(double actual, double expected) { assert(std::abs(actual - expected) < 1e-8); }
void append_crc(Bytes& bytes) {
    const auto crc = meter::crc16(bytes.data(), bytes.size());
    bytes.push_back(crc & 255); bytes.push_back(crc >> 8);
}
// Independent synthetic frame construction; anonymized fixture CRCs are checked independently.
Bytes frame(Bytes info, bool segmented = false, std::uint8_t control = 0x03,
            std::uint8_t source = 0x21) {
    const auto size = (info.empty() ? 7 : 9) + info.size();
    assert(size <= 2047);
    Bytes result{static_cast<std::uint8_t>(0xa0 | (segmented ? 8 : 0) | (size >> 8)),
                 static_cast<std::uint8_t>(size), 0x03, source, control};
    if (!info.empty()) {
        append_crc(result); result.insert(result.end(), info.begin(), info.end());
    }
    append_crc(result); result.insert(result.begin(), 0x7e); result.push_back(0x7e);
    return result;
}
Bytes notification(Bytes value) {
    Bytes p{0xe6,0xe7,0,0x0f,0,0,0,1,0};
    p.insert(p.end(), value.begin(), value.end()); return p;
}
void direct(meter::Processor& processor, const Bytes& bytes, std::uint64_t now) {
    processor.feed(bytes.data(), bytes.size(), false, now); processor.finish(now);
}
void real_captures() {
    const auto expected_apdu = hex(file("test/fixtures/isk-1-llc.txt"));
    const double powers[]{950,945,940,965};
    const double energies[]{9281471,9281473,9281474,9281475};
    const double tariff2[]{7743741,7743743,7743744,7743745};
    const double voltages[][3]{{241.7,239.9,242.6},{241.8,240,242.6},{241.6,240,242.5},{241.8,240.1,242.6}};
    const double currents[][3]{{.3,3,1.31},{.31,2.85,1.30},{.3,2.77,1.28},{.31,2.89,1.31}};
    for (unsigned index = 1; index <= 4; ++index) {
        const auto bytes = wire(index);
        for (std::size_t chunk : {1,2,3,7,31,64,164,166,256}) {
            meter::Processor p; feed(p, bytes, chunk); p.finish(100);
            assert(p.counters().errors == 0 && p.counters().notifications == 1);
            assert(p.counters().validated_hdlc == 1 && p.readings().size() == 16);
            equal(sample(p,1,7).reading.scaled, powers[index-1]);
            equal(sample(p,2,7).reading.scaled, 0);
            equal(sample(p,1,8).reading.scaled, energies[index-1]);
            equal(sample(p,2,8).reading.scaled, 5876412);
            equal(sample(p,1,8,1).reading.scaled, 1537730);
            equal(sample(p,1,8,2).reading.scaled, tariff2[index-1]);
            equal(sample(p,2,8,1).reading.scaled, 154430);
            equal(sample(p,2,8,2).reading.scaled, 5721982);
            for (unsigned phase = 0; phase < 3; ++phase) {
                const auto& v = sample(p,32+20*phase,7).reading;
                const auto& a = sample(p,31+20*phase,7).reading;
                equal(v.scaled, voltages[index-1][phase]); equal(a.scaled, currents[index-1][phase]);
                assert(v.type == 18 && v.scaler == -1 && v.unit == 35);
                assert(a.scaler == -2 && a.unit == 33);
            }
            assert(p.last_document().invoke_id == 0x009d4100 + index);
            assert(p.last_document().datetime.size() == 12);
            const auto& id = p.readings()[0];
            assert(!id.reading.numeric && id.encoded_value == hex("09083132333435363738"));
            assert(p.readings()[1].encoded_value == hex("090731323334353637"));
            if (index == 1) assert(p.last_document().encoded == expected_apdu);
        }
    }
    // Every single transport split, including splits in format/address/HCS/FCS.
    for (unsigned index = 1; index <= 4; ++index) {
    const auto bytes = wire(index);
    for (std::size_t split = 0; split <= bytes.size(); ++split) {
        meter::Processor p;
        if (split) p.feed(bytes.data(), split, true, 10);
        if (split < bytes.size()) p.feed(bytes.data()+split, bytes.size()-split, true, 11);
        p.finish(11);
        assert(p.counters().notifications == 1 && p.counters().errors == 0);
        equal(sample(p,1,7).reading.scaled, powers[index-1]);
        if (index == 1) assert(p.last_document().encoded == expected_apdu);
    }
    }
    const auto bytes = wire(1);
    // Verify reassembly callback directly against separately supplied /hdlc bytes.
    unsigned packets = 0;
    meter::Hdlc parser([&](const Bytes& apdu, std::uint64_t) { assert(apdu == expected_apdu); ++packets; },
                       [](meter::Error, std::uint64_t) { assert(false); });
    parser.feed(bytes.data(), bytes.size(), 10); parser.finish(10); assert(packets == 1);
    meter::Processor p;
    for (unsigned i=1; i<=4; ++i) feed(p, wire(i), 23, i*100);
    assert(p.counters().notifications == 4 && p.counters().errors == 0);
    equal(sample(p,1,7).reading.scaled, 965);
    // Explicit LLC input produces the same document without a validated-HDLC claim.
    meter::Processor llc;
    for (const auto byte : expected_apdu) llc.feed(&byte, 1, false, 100);
    llc.finish(100);
    assert(llc.counters().validated_hdlc == 0 && llc.counters().notifications == 1);
    assert(llc.last_document().encoded == expected_apdu);
    feed(llc, expected_apdu, 1, 200); llc.finish(200);
    assert(llc.counters().notifications == 1); // APDU bytes never become valid HDLC.
}
void errors_and_segments() {
    const auto good = wire(1);
    meter::Processor p; feed(p, good, 17); p.finish(100);
    auto corrupted = good; corrupted[40] ^= 1;
    feed(p, corrupted, 1, 200); p.finish(200);
    assert(p.counters().notifications == 1 && p.counters().errors > 0);
    equal(sample(p,1,7).reading.scaled, 950);
    assert(sample(p,1,7).reading.t_ms == 100);
    feed(p, wire(2), 23, 300); p.finish(300);
    equal(sample(p,1,7).reading.scaled, 945);
    // Incomplete and dropped UI segments cannot publish partially decoded readings.
    for (auto bad : {Bytes(good.begin(), good.end()-1), Bytes(good.begin(), good.begin()+100),
                     Bytes(good.begin(), good.begin()+166)}) {
        meter::Processor q; feed(q,bad,1); q.finish(100);
        assert(q.counters().errors > 0 && q.counters().notifications == 0);
    }
    auto missing_middle = Bytes(good.begin(), good.begin()+166);
    missing_middle.insert(missing_middle.end(), good.begin()+332, good.end());
    meter::Processor missing; feed(missing, missing_middle, 7); missing.finish(100);
    assert(missing.counters().notifications == 0 && missing.counters().errors > 0);
    // Reordered, individually CRC-valid UI segments still fail A-XDR decoding.
    auto reordered = Bytes(good.begin()+166, good.begin()+332);
    reordered.insert(reordered.end(), good.begin(), good.begin()+166);
    reordered.insert(reordered.end(), good.begin()+332, good.end());
    meter::Processor order; feed(order,reordered,7); order.finish(100);
    assert(order.counters().notifications == 0 && order.counters().errors > 0);
    auto apdu = hex(file("test/fixtures/isk-1-llc.txt"));
    auto a = frame(Bytes(apdu.begin(),apdu.begin()+100),true,0x0e); // N(S)=7
    auto b = frame(Bytes(apdu.begin()+100,apdu.end()),false,0x00); // wraps to 0
    meter::Processor segmented; feed(segmented,a,1); feed(segmented,b,1,101);
    assert(segmented.counters().notifications == 1);
    for (auto bad : {frame(Bytes(apdu.begin()+100,apdu.end()),false,0x0e),
                     frame(Bytes(apdu.begin()+100,apdu.end()),false,0x00,0x23)}) {
        meter::Processor q; feed(q,a,1); feed(q,bad,1,101);
        assert(q.counters().last_error == meter::Error::Sequence && q.counters().notifications == 0);
    }
    // A legal RR between segments is not an application frame or a reset.
    meter::Processor rr; feed(rr,a,1); feed(rr,frame({},false,0x11),1,101); feed(rr,b,1,102);
    assert(rr.counters().notifications == 1);
    meter::Processor timeout; feed(timeout,a,1); timeout.tick(2100);
    assert(timeout.counters().last_error == meter::Error::Timeout);
    feed(timeout,b,1,2101); assert(timeout.counters().notifications == 0);
    meter::Processor idle;
    idle.feed(good.data(),10,true,0); idle.tick(1999); assert(idle.counters().errors == 0);
    idle.feed(good.data()+10,10,true,1999); idle.tick(3998); assert(idle.counters().errors == 0);
    idle.tick(3999); assert(idle.counters().last_error == meter::Error::Timeout);
    // UART errors invalidate in-progress reassembly, not last good samples.
    meter::Processor discontinuity; feed(discontinuity,good,32); feed(discontinuity,a,1,101);
    discontinuity.discontinuity(102); feed(discontinuity,b,1,103);
    assert(discontinuity.counters().notifications == 1);
    equal(sample(discontinuity,1,7).reading.scaled,950);
    // Frame validation: independent known CRC vector and supplied HCS/FCS.
    const std::string check="123456789";
    assert(meter::crc16(reinterpret_cast<const std::uint8_t*>(check.data()), check.size()) == 0x906e);
    meter::Frame output;
    auto f=frame({0x7e,0,0x7e});
    assert(meter::validate_frame(f.data()+1,f.size()-2,output) == meter::Error::None);
    assert(output.information == Bytes({0x7e,0,0x7e}));
    f[f.size()-3] ^= 1;
    assert(meter::validate_frame(f.data()+1,f.size()-2,output) == meter::Error::Fcs);
    f=frame({1,2,3}); f[6] ^= 1;
    const auto fcs=meter::crc16(f.data()+1,f.size()-4);
    f[f.size()-3]=fcs&255; f[f.size()-2]=fcs>>8;
    assert(meter::validate_frame(f.data()+1,f.size()-2,output) == meter::Error::Hcs);
    f=frame({1}); f[2]++;
    assert(meter::validate_frame(f.data()+1,f.size()-2,output) == meter::Error::Length);
    f=frame({1}); f[1]=0xb0;
    assert(meter::validate_frame(f.data()+1,f.size()-2,output) == meter::Error::Format);
    assert(meter::validate_frame(nullptr,0,output) == meter::Error::Length);
    f=frame({1}); f[3]=2; f[4]=2; f[5]=3;
    assert(meter::validate_frame(f.data()+1,f.size()-2,output) == meter::Error::Address);
    meter::Processor over;
    auto long_frame=frame(Bytes(1800,0),true,0x03);
    for (int i=0;i<5;++i) feed(over,long_frame,256,100+i);
    assert(over.counters().last_error == meter::Error::Overflow);
    feed(over,frame({1},true,0x53),1,200);
    assert(over.counters().last_error == meter::Error::Control);
    // Shared end/start flag and idle flag runs are legal.
    auto simple=frame(apdu); auto twice=simple;
    twice.insert(twice.end(),simple.begin()+1,simple.end());
    meter::Processor shared; feed(shared,twice,1);
    assert(shared.counters().notifications == 2);
    // No information frame: one FCS, no HCS.
    f=frame({}); assert(meter::validate_frame(f.data()+1,f.size()-2,output)==meter::Error::None);
    assert(output.information.empty());
}
void axdr_and_objects() {
    meter::Document d;
    assert(meter::decode_notification(notification(hex("020306ffffffff0fff0fff")),d));
    double v=0;
    assert(meter::number(d,d.values[d.root].children[0],v)); equal(v,4294967295.0);
    assert(meter::number(d,d.values[d.root].children[1],v)); equal(v,-1);
    assert(meter::number(d,d.values[d.root].children[2],v)); equal(v,-1);
    for (auto value : {"148000000000000000","15ffffffffffffffff","173fc00000","183ff8000000000000"}) {
        assert(meter::decode_notification(notification(hex(value)),d));
        assert(meter::number(d,d.root,v));
    }
    for (auto value : {"037f","0903aa","018001","01820001ff","ff","1901","0201"}) {
        assert(!meter::decode_notification(notification(hex(value)),d));
    }
    auto deep=Bytes{};
    for(int i=0;i<14;++i) {deep.push_back(2);deep.push_back(1);} deep.push_back(0);
    assert(!meter::decode_notification(notification(deep),d));
    assert(!meter::decode_notification(notification(Bytes(9000,0)),d));
    auto trailing=notification({0});trailing.push_back(0);
    assert(!meter::decode_notification(trailing,d));
    auto text=Bytes{9,0x82,4,1}; text.resize(4+1025,0);
    assert(!meter::decode_notification(notification(text),d));
    auto valid_text=Bytes{9,0x81,128};valid_text.resize(131,42);
    assert(meter::decode_notification(notification(valid_text),d));
    // Unknown OBIS and unit preserved; no hardcoded scaler inferred.
    auto unknown=notification(hex("0201020309060100fa0700ff060000006402020fff16c8"));
    meter::Processor p; direct(p,unknown,100);
    assert(p.readings().size()==1);
    const auto& r=p.readings()[0].reading;
    assert(!meter::known_name(r.obis) && r.unit==200 && r.scaler==-1); equal(r.scaled,10);
    assert(p.readings()[0].encoded_value==hex("0600000064"));
    auto view=display::make_view(p,true,100);
    assert(!view.power_valid);
    auto duplicate=notification(hex(
        "0202020209060100fa0700ff112a020209060100fa0700ff112b"));
    meter::Processor ambiguous;direct(ambiguous,duplicate,100);
    assert(ambiguous.readings().empty() && ambiguous.counters().last_error==meter::Error::Objects);
    // Unsupported A-XDR tag preserves last reading, and NaN cannot replace a good number.
    direct(p,notification(hex("ff")),200);
    assert(p.counters().notifications==1 && p.readings()[0].reading.t_ms==100);
    direct(p,notification(hex("0201020309060100fa0700ff177f80000002020fff16c8")),300);
    assert(p.counters().notifications==1 && p.readings()[0].reading.t_ms==100);
    // An explicit descriptor list supplies value/scaler association, no positional guesses.
    const auto push=notification(hex(
        "02030102020412000309060100010700ff0f02120000"
        "020412000309060100010700ff0f03120000"
        "06000003e802020fff161b"));
    meter::Processor standard;direct(standard,push,100);
    assert(standard.counters().errors==0 && standard.readings().size()==1);
    equal(sample(standard,1,7).reading.scaled,100);
    assert(sample(standard,1,7).reading.class_id==3);
    // Standard push: the first descriptor describes the list itself (class 40, attribute 2).
    const auto self_push=notification(hex(
        "02030103020412002809060000190900ff0f02120000"
        "020412000309060100010700ff0f02120000"
        "020412000309060100010700ff0f03120000"
        "06000003e802020fff161b"));
    meter::Processor self;direct(self,self_push,100);
    assert(self.counters().errors==0 && self.readings().size()==1);
    equal(sample(self,1,7).reading.scaled,100);
    const auto duplicate_scaler=notification(hex(
        "02040103020412000309060100010700ff0f02120000"
        "020412000309060100010700ff0f03120000"
        "020412000309060100010700ff0f03120000"
        "06000003e802020fff161b02020f00161b"));
    meter::Processor repeated;direct(repeated,duplicate_scaler,100);
    assert(repeated.readings().empty() && repeated.counters().last_error==meter::Error::Objects);
    // A tuple with no scaler stays raw; missing data never invents a unit.
    meter::Processor raw;direct(raw,notification(hex("0201020209060100010700ff06000003e8")),100);
    assert(!sample(raw,1,7).reading.has_scaler_unit);
    view=display::make_view(raw,false,100);
    assert(!view.power_valid);
    direct(raw,notification(hex("0201020209060100010700ff00")),200);
    equal(sample(raw,1,7).reading.raw_number,1000);
    assert(sample(raw,1,7).reading.t_ms==100);
    // Unknown but valid payload schema remains inspectable without guessed readings.
    meter::Processor unmapped;direct(unmapped,notification(hex("0202060000002a0906010203040506")),100);
    assert(unmapped.counters().notifications==1 && unmapped.readings().empty());
    assert(unmapped.last_document().values.size()==3);
}
void limits() {
    meter::Document d;
    Bytes nodes{1,0x81,128};
    for(unsigned i=0;i<128;++i) {nodes.push_back(2);nodes.push_back(1);nodes.push_back(0);}
    assert(!meter::decode_notification(notification(nodes),d)); // 257 nodes
    assert(!meter::decode_notification(notification({1,0x81,129}),d));
    auto tuples=[](unsigned count, unsigned start) {
        Bytes values{2,static_cast<std::uint8_t>(count)};
        for(unsigned i=0;i<count;++i) {
            Bytes entry{2,2,9,6,1,0,static_cast<std::uint8_t>(start+i),7,0,255,17,42};
            values.insert(values.end(),entry.begin(),entry.end());
        }
        return notification(values);
    };
    meter::Processor p;direct(p,tuples(64,100),100);
    assert(p.readings().size()==64 && p.counters().notifications==1);
    direct(p,tuples(1,200),200);
    assert(p.readings().size()==64 && p.counters().last_error==meter::Error::Overflow);
    meter::Processor many;direct(many,tuples(65,100),100);
    assert(many.readings().empty() && many.counters().errors==1);
    meter::Processor bytes;
    for(unsigned i=0;i<8;++i) {
        Bytes values{2,1,2,2,9,6,1,0,static_cast<std::uint8_t>(100+i),7,0,255,9,0x82,4,0};
        values.resize(values.size()+1024,0x41);
        direct(bytes,notification(values),i*100);
    }
    assert(bytes.readings().size()==7 && bytes.counters().last_error==meter::Error::Overflow);
    Bytes oversized(meter::Hdlc::max_apdu+1,0);
    meter::Processor direct_limit;direct(direct_limit,oversized,100);
    assert(direct_limit.counters().last_error==meter::Error::Overflow);
}
void pipeline_and_view() {
    input::Pipeline p;p.select(input::Source::Replay);
    input::Replay replay;std::string error;
    assert(replay.load(file("test/fixtures/isk-1-wire.txt"),input::Layer::WireHdlc,error));
    while(!replay.done())assert(replay.next(p,16,100));
    assert(p.processor().counters().notifications==1);
    auto view=display::make_view(p.processor(),true,100);
    assert(std::string(view.lines[0].data())=="Simulation");
    assert(view.power_valid&&std::string(view.power.data())=="950"&&std::string(view.flow.data())=="Import");
    assert(std::string(view.voltage[0].data())=="241.7V");
    assert(std::string(view.voltage[1].data())=="239.9V");
    assert(std::string(view.voltage[2].data())=="242.6V");
    auto stale=display::make_view(p.processor(),true,20000);
    assert(std::string(stale.lines[1].data()).find("Stale")==0);
    assert(stale.power_stale&&stale.voltage_stale[0]);
    std::vector<std::uint16_t> pixels(240*135+2,0x1234);
    display::render(stale,pixels.data()+1);
    assert(pixels.front()==0x1234 && pixels.back()==0x1234);
    for(bool rotate:{false,true}){
        std::array<std::uint16_t,240*display::transfer_rows+2> band{};
        for(std::size_t first=0;first<135;first+=display::transfer_rows){
            const auto rows=std::min(display::transfer_rows,135-first);
            band.fill(0x1234);display::render_rows(stale,band.data()+1,first,rows,rotate);
            assert(band.front()==0x1234&&band[1+240*rows]==0x1234);
            for(std::size_t n=0;n<240*rows;++n){const auto index=first*240+n;
                assert(band[n+1]==pixels[1+(rotate?240*135-1-index:index)]);}
        }
    }

    std::vector<std::uint16_t> fresh_pixels(240*135);display::render(view,fresh_pixels.data());
    assert(std::find(fresh_pixels.begin(),fresh_pixels.end(),0xffff)!=fresh_pixels.end());
    assert(std::find_if(pixels.begin()+1,pixels.end()-1,[&](auto color){return color!=pixels[1]&&color!=0xffff;})!=pixels.end()-1);
    display::ReceptionPulse pulse;assert(!pulse.active(0));pulse.update(1,100);assert(pulse.active(100)&&pulse.active(499)&&!pulse.active(500));pulse.update(1,300);assert(!pulse.active(500));pulse.update(2,600);assert(pulse.active(999));pulse.update(0,650);assert(!pulse.active(650));
    meter::Processor exporting;direct(exporting,notification(hex("0202020309060100010700ff060000006402020f00161b020309060100020700ff06000009c402020f00161b")),100);
    const auto export_view=display::make_view(exporting,false,100);assert(export_view.power_valid&&export_view.exporting&&std::string(export_view.power.data())=="2.40"&&std::string(export_view.unit.data())=="kW");
    meter::Processor import_only;direct(import_only,notification(hex("0201020309060100010700ff06000003e802020f00161b")),100);assert(display::make_view(import_only,false,100).power_valid);
    meter::Processor export_only;direct(export_only,notification(hex("0201020309060100020700ff06000003e802020f00161b")),100);assert(!display::make_view(export_only,false,100).power_valid);
    auto icons=view;icons.wifi_connected=true;icons.wifi_bars=3;icons.receiving=true;icons.mqtt=display::MqttState::Connected;std::vector<std::uint16_t> lit(240*135);display::render(icons,lit.data());assert(lit[10*240+210]!=fresh_pixels[10*240+210]);assert(lit[7*240+154]!=fresh_pixels[7*240+154]);
    const auto bytes=wire(1);Bytes retained;
    for(std::size_t i=0;i<p.size();++i)retained.insert(retained.end(),p.at(i).bytes.begin(),p.at(i).bytes.begin()+p.at(i).size);
    assert(retained==bytes); // raw capture is before validation, unchanged.
    p.select(input::Source::Uart);
    assert(p.processor().readings().empty() && p.processor().counters().notifications==0);
    view=display::make_view(p.processor(),false,100);
    assert(std::string(view.lines[1].data()).find("No readings")==0);
    // Decoder failure leaves the malformed original chunk in raw history.
    std::uint8_t bad[]{0x7e,0xa0,0};
    assert(p.ingest(input::Source::Uart,input::Layer::WireHdlc,bad,3,200));
    assert(p.processor().counters().errors>0);
    assert(std::equal(bad,bad+3,p.at(p.size()-1).bytes.begin()));
}
void fuzz_bounded() {
    std::uint32_t state=0x12345678;
    for(unsigned trial=0;trial<1000;++trial) {
        Bytes bytes(trial%512);
        for(auto& byte:bytes) {state^=state<<13;state^=state>>17;state^=state<<5;byte=state&255;}
        meter::Processor p;feed(p,bytes,17);p.finish(100);
        meter::Document d;
        meter::decode_notification(notification(bytes),d);
        if(!d.values.empty()){std::vector<meter::Object> objects;meter::extract_objects(d,objects);}
    }
}
int main() {
    real_captures();errors_and_segments();axdr_and_objects();limits();pipeline_and_view();fuzz_bounded();
}
