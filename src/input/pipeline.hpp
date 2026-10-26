#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "meter/protocol.hpp"

namespace openp1::input {
enum class Parity { None, Even, Odd };
enum class StopBits { One, Two };
struct SerialSettings {
    std::uint32_t baud = 115200;
    Parity parity = Parity::None;
    StopBits stop_bits = StopBits::One;
    bool inverted = true;
    bool valid() const;
};
enum class Source { Uart, Replay };
enum class Layer { WireHdlc, LlcApdu };
struct Chunk {
    std::array<std::uint8_t, 256> bytes{};
    std::size_t size = 0;
    std::uint64_t t_ms = 0;
    Source source = Source::Uart;
    Layer layer = Layer::WireHdlc;
};
// Single owner: the acquisition task. Consumers must request a safe snapshot.
class Pipeline {
public:
    Pipeline() = default;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    void select(Source source) { if (source != source_) processor_.reset(); source_ = source; }
    Source source() const { return source_; }
    void observe(meter::Hdlc::WireFrame frame,meter::Hdlc::Packet packet){processor_.observe(std::move(frame),std::move(packet));}
    bool ingest(Source source, Layer layer, const std::uint8_t* data,
                std::size_t size, std::uint64_t t_ms);
    const Chunk& at(std::size_t index) const;
    std::size_t size() const { return count_; }
    std::uint64_t overwritten() const { return overwritten_; }
    void uart_error(std::uint64_t now = 0) { ++uart_errors_; processor_.discontinuity(now); }
    void tick(std::uint64_t now) { processor_.tick(now); }
    void finish(std::uint64_t now) { processor_.finish(now); }
    const meter::Processor& processor() const { return processor_; }
    std::uint64_t uart_errors() const { return uart_errors_; }
private:
    meter::Processor processor_;
    std::array<Chunk, 32> history_{};
    std::size_t begin_ = 0, count_ = 0;
    Source source_ = Source::Uart;
    std::uint64_t overwritten_ = 0, uart_errors_ = 0;
};
// M1 supports hex lines and '<topic> <hex>' logs. JSONL is rejected until M5.
// Caller must identify the layer; no wrapper or synthetic CRC is ever added.
class Replay {
public:
    static constexpr std::size_t max_text = 65536;
    static constexpr std::size_t max_bytes = 16384;
    bool load(std::string_view text, Layer layer, std::string& error);
    bool next(Pipeline& pipeline, std::size_t chunk_size, std::uint64_t t_ms);
    bool done() const { return position_ == bytes_.size(); }
    void rewind() { position_ = 0; }
private:
    std::vector<std::uint8_t> bytes_;
    std::size_t position_ = 0;
    Layer layer_ = Layer::WireHdlc;
};
}
