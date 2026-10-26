#include "pipeline.hpp"
#include "meter/hex.hpp"
#include <algorithm>
#include <cassert>

namespace openp1::input {
bool SerialSettings::valid() const {
    return baud >= 300 && baud <= 115200 &&
        (parity == Parity::None || parity == Parity::Even || parity == Parity::Odd) &&
        (stop_bits == StopBits::One || stop_bits == StopBits::Two);
}
bool Pipeline::ingest(Source source, Layer layer, const std::uint8_t* data,
                      std::size_t size, std::uint64_t t_ms) {
    if (source != source_ || !data || size == 0 || size > 256) return false;
    std::size_t slot = (begin_ + count_) % history_.size();
    if (count_ == history_.size()) {
        begin_ = (begin_ + 1) % history_.size();
        ++overwritten_;
    } else ++count_;
    auto& chunk = history_[slot];
    std::copy_n(data, size, chunk.bytes.begin());
    chunk.size = size;
    chunk.t_ms = t_ms;
    chunk.source = source;
    chunk.layer = layer;
    processor_.feed(data, size, layer == Layer::WireHdlc, t_ms);
    return true;
}
const Chunk& Pipeline::at(std::size_t index) const {
    assert(index < count_);
    return history_[(begin_ + index) % history_.size()];
}
bool Replay::load(std::string_view text, Layer layer, std::string& error) {
    error.clear();
    // Failed loads clear the previous replay, so stale data cannot be replayed.
    bytes_.clear(); position_ = 0;
    if (text.size() > max_text) { error = "replay exceeds 65536 text bytes"; return false; }
    std::vector<std::uint8_t> parsed;
    std::size_t line_number = 0;
    while (!text.empty()) {
        ++line_number;
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        const auto first = line.find_first_not_of(" \t\r");
        if (first == line.npos) continue;
        line.remove_prefix(first);
        auto fail = [&](const char* reason) {
            error = "line " + std::to_string(line_number) + ": " + reason;
            return false;
        };
        if (line.size() > 4096) return fail("line exceeds 4096 bytes");
        if (line.front() == '{') return fail("JSONL capture requires M5 importer");
        // Topic wrappers must contain a slash, avoiding ambiguous bare words.
        const auto space = line.find_first_of(" \t");
        const auto topic = line.substr(0, space);
        if (topic.find('/') != topic.npos) {
            if (space == line.npos) return fail("missing MQTT payload");
            line.remove_prefix(space);
        }
        std::vector<std::uint8_t> decoded;
        if (!meter::parse_hex(line, decoded)) return fail("invalid hex or unsupported wrapper");
        if (decoded.size() > max_bytes - parsed.size()) return fail("replay exceeds 16384 raw bytes");
        parsed.insert(parsed.end(), decoded.begin(), decoded.end());
    }
    if (parsed.empty()) { error = "empty replay"; return false; }
    bytes_ = std::move(parsed); layer_ = layer;
    return true;
}
bool Replay::next(Pipeline& pipeline, std::size_t chunk_size, std::uint64_t t_ms) {
    if (chunk_size == 0 || chunk_size > 256 || done()) return false;
    if (position_ == 0 && pipeline.source() == Source::Replay) pipeline.finish(t_ms);
    const auto count = std::min(chunk_size, bytes_.size() - position_);
    if (!pipeline.ingest(Source::Replay, layer_, bytes_.data() + position_, count, t_ms)) return false;
    position_ += count;
    if (done()) pipeline.finish(t_ms);
    return true;
}
}
