#include "meter/presentation.hpp"
#include <cassert>
int main() {
    using namespace openp1::meter;
    assert(obis_display({1,1,1,8,1,255})=="1-1:1.8.1");
    assert(obis_display({1,0,1,8,2,0})=="1-0:1.8.2*0");
    for(unsigned tariff=1;tariff<255;++tariff) {
        const auto t=static_cast<std::uint8_t>(tariff);
        assert(reading_label({1,0,1,8,t,255})=="Import +A tariff "+std::to_string(tariff));
        assert(reading_label({1,1,2,8,t,255})=="Export -A tariff "+std::to_string(tariff));
    }
    assert(reading_label({1,0,1,8,0,255})=="Import +A total");
    assert(reading_label({0,0,96,1,0,255})=="Serial number");
    assert(reading_label({0,0,96,1,1,255})=="GR number");
    std::string out;bool binary=true;
    const Bytes serial{9,8,'0','0','0','1','2','3','4','5'};
    assert(string_value(serial.data(),serial.size(),out,binary)&&!binary&&out=="00012345");
    const Bytes utf8{12,2,0xc3,0xa9};
    assert(string_value(utf8.data(),utf8.size(),out,binary)&&!binary&&out=="\xC3\xA9");
    const Bytes invalid{9,2,0xff,0};
    assert(string_value(invalid.data(),invalid.size(),out,binary)&&binary&&out=="FF00");
    const Bytes overlong{10,2,0xc0,0x80};
    assert(string_value(overlong.data(),overlong.size(),out,binary)&&binary);
    const Bytes truncated{9,3,'a'};
    assert(!string_value(truncated.data(),truncated.size(),out,binary));
    Bytes long_text{9,0x82,4,0};long_text.insert(long_text.end(),1024,'a');
    assert(string_value(long_text.data(),long_text.size(),out,binary)&&out.size()==1024&&!binary);
    long_text[3]=1;long_text.push_back('a');assert(!string_value(long_text.data(),long_text.size(),out,binary));
}
