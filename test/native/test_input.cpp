#include "input/pipeline.hpp"
#include <cassert>
#include <vector>
using namespace openp1::input;
int main() {
    SerialSettings serial;
    assert(serial.valid() && serial.inverted && serial.baud == 115200);
    for (auto parity : {Parity::None, Parity::Odd, Parity::Even}) {
        for (auto stop : {StopBits::One, StopBits::Two}) {
            serial.parity = parity; serial.stop_bits = stop;
            serial.inverted = false; assert(serial.valid());
            serial.inverted = true; assert(serial.valid());
        }
    }
    serial.baud = 0; assert(!serial.valid());
    serial.baud = 115201; assert(!serial.valid());
    serial.baud = 115200; serial.parity = static_cast<Parity>(99); assert(!serial.valid());
    serial.parity = Parity::None; serial.stop_bits = static_cast<StopBits>(99); assert(!serial.valid());
    Replay replay;
    std::string error;
    // Includes embedded flags, truncated and checksum-invalid bytes: preserve all.
    const std::vector<std::uint8_t> expected{0x7e, 0xa8, 0x7e, 0, 0xff, 0x7e, 1};
    for (std::size_t n : {1, 2, 3, 7, 256}) {
        Pipeline pipeline;
        assert(replay.load("7e a8 7e\nopenp1/raw 00ff7e01\r\n", Layer::WireHdlc, error));
        assert(!replay.next(pipeline, n, 0)); // UART is still selected.
        pipeline.select(Source::Replay);
        while (!replay.done()) assert(replay.next(pipeline, n, 42));
        std::vector<std::uint8_t> actual;
        for (std::size_t i = 0; i < pipeline.size(); ++i) {
            const auto& c = pipeline.at(i);
            assert(c.t_ms == 42 && c.source == Source::Replay && c.layer == Layer::WireHdlc);
            actual.insert(actual.end(), c.bytes.begin(), c.bytes.begin() + c.size);
        }
        assert(actual == expected);
        assert(!replay.next(pipeline, n, 42));
        const auto previous_size=pipeline.size();
        replay.rewind();
        while(!replay.done())assert(replay.next(pipeline,n,3542));
        actual.clear();
        for(std::size_t i=previous_size;i<pipeline.size();++i){
            const auto& c=pipeline.at(i);
            assert(c.t_ms==3542&&c.source==Source::Replay&&c.layer==Layer::WireHdlc);
            actual.insert(actual.end(),c.bytes.begin(),c.bytes.begin()+c.size);
        }
        assert(actual==expected);

    }
    Pipeline pipeline;
    pipeline.select(Source::Replay);
    assert(replay.load("e6e700", Layer::LlcApdu, error));
    assert(!replay.next(pipeline, 0, 0));
    assert(!replay.next(pipeline, 257, 0));
    assert(replay.next(pipeline, 256, 0));
    assert(pipeline.at(0).layer == Layer::LlcApdu && pipeline.at(0).bytes[0] == 0xe6);
    for (auto bad : {"", "7", "7e zz", "topic/raw", "{\"type\":\"rx\"}", "7e\nzz"}) {
        assert(!replay.load(bad, Layer::WireHdlc, error));
        assert(!error.empty() && replay.done());
    }
    assert(error.find("line 2:") == 0);
    assert(!replay.load(std::string(65537, ' '), Layer::WireHdlc, error));
    assert(!replay.load(std::string(4097, 'a'), Layer::WireHdlc, error));
    std::string oversized;
    for (int i = 0; i < 17; ++i) oversized += std::string(2048, 'a') + '\n';
    assert(!replay.load(oversized, Layer::WireHdlc, error));
    pipeline.select(Source::Uart);
    std::uint8_t bytes[257]{};
    assert(!pipeline.ingest(Source::Replay, Layer::WireHdlc, bytes, 1, 0));
    assert(!pipeline.ingest(Source::Uart, Layer::WireHdlc, bytes, 257, 0));
    assert(!pipeline.ingest(Source::Uart, Layer::WireHdlc, nullptr, 1, 0));
    const auto initial = pipeline.size();
    for (int i = 0; i < 40; ++i) {
        bytes[0] = static_cast<std::uint8_t>(i);
        assert(pipeline.ingest(Source::Uart, Layer::WireHdlc, bytes, 1, i));
    }
    assert(pipeline.size() == 32 && pipeline.overwritten() == 40 + initial - 32);
    assert(pipeline.at(0).bytes[0] == 8 && pipeline.at(31).bytes[0] == 39);
    pipeline.uart_error(); assert(pipeline.uart_errors() == 1);
}
