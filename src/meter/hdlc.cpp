#include "protocol.hpp"
#include <utility>

namespace openp1::meter {
std::uint16_t crc16(const std::uint8_t* data, std::size_t size) {
    std::uint16_t crc = 0xffff;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = static_cast<std::uint16_t>((crc >> 1) ^ ((crc & 1) ? 0x8408 : 0));
    }
    return static_cast<std::uint16_t>(crc ^ 0xffff);
}
const char* error_name(Error e) {
    switch (e) {
    case Error::None: return "none";
    case Error::Format: return "HDLC format";
    case Error::Length: return "HDLC length";
    case Error::Hcs: return "bad HCS";
    case Error::Fcs: return "bad FCS";
    case Error::Address: return "HDLC address";
    case Error::Control: return "HDLC control";
    case Error::Sequence: return "segment sequence";
    case Error::Overflow: return "limit exceeded";
    case Error::Timeout: return "input timeout";
    case Error::Truncated: return "truncated input";
    case Error::Llc: return "unsupported LLC";
    case Error::Apdu: return "unsupported APDU";
    case Error::Axdr: return "invalid A-XDR";
    case Error::Objects: return "object mapping";
    case Error::Memory: return "memory exhausted";
    case Error::Discontinuity: return "UART discontinuity";
    }
    return "unknown";
}
namespace {
bool checksum(const std::uint8_t* data, std::size_t count) {
    if (count < 2) return false;
    const auto crc = crc16(data, count - 2);
    return data[count - 2] == (crc & 0xff) && data[count - 1] == (crc >> 8);
}
bool address(const std::uint8_t* data, std::size_t size, std::size_t& pos,
             std::uint32_t& result) {
    result = 0;
    for (unsigned n = 1; n <= 4 && pos < size; ++n) {
        const auto byte = data[pos++];
        result = (result << 7) | (byte >> 1);
        if (byte & 1) return n == 1 || n == 2 || n == 4;
    }
    return false;
}
}
Error validate_frame(const std::uint8_t* body, std::size_t size, Frame& output) {
    if (!body || size < 7 || size > 2047) return Error::Length;
    if ((body[0] & 0xf0) != 0xa0) return Error::Format;
    if ((((body[0] & 7u) << 8) | body[1]) != size) return Error::Length;
    Frame frame;
    frame.segmented = (body[0] & 8) != 0;
    std::size_t pos = 2;
    if (!address(body, size - 3, pos, frame.destination) ||
        !address(body, size - 3, pos, frame.source)) return Error::Address;
    if (pos >= size - 2) return Error::Length;
    frame.control = body[pos++];
    if (!checksum(body, size)) return Error::Fcs;
    if (pos == size - 2) {
        // A frame without information has FCS only, no HCS.
        if (frame.segmented) return Error::Control;
    } else {
        if (size - pos < 5) return Error::Length; // HCS, >=1 information byte, FCS
        if (!checksum(body, pos + 2)) return Error::Hcs;
        pos += 2;
        frame.information.assign(body + pos, body + size - 2);
    }
    output = std::move(frame);
    return Error::None;
}
Hdlc::Hdlc(Packet packet, Failure failure) : packet_(std::move(packet)), failure_(std::move(failure)) {
    frame_.reserve(2048); assembled_.reserve(max_apdu);
}
void Hdlc::reset() {
    frame_.clear(); assembled_.clear(); expected_ = 0;
    started_ = false; assembling_ = false;
}
void Hdlc::fail(Error e, std::uint64_t now) {
    reset(); failure_(e, now);
}
void Hdlc::tick(std::uint64_t now) {
    if ((!frame_.empty() && now - frame_time_ >= 2000) ||
        (assembling_ && now - segment_time_ >= 2000)) fail(Error::Timeout, now);
}
void Hdlc::finish(std::uint64_t now) {
    if (!frame_.empty() || assembling_) fail(Error::Truncated, now);
    else reset();
}
void Hdlc::feed(const std::uint8_t* data, std::size_t size, std::uint64_t now) {
    tick(now);
    for (std::size_t i = 0; i < size; ++i) {
        const auto byte = data[i];
        if (!started_) {
            if (byte == 0x7e) started_ = true;
            continue;
        }
        if (frame_.empty() && byte == 0x7e) continue; // idle/shared flags
        frame_time_ = now;
        if (assembling_) segment_time_ = now;
        // Only the declared end can be a closing flag: 0x7e inside a body is data.
        if (expected_ && frame_.size() == expected_) {
            if (byte != 0x7e) { fail(Error::Length, now); continue; }
            if(wire_frame_)wire_frame_(frame_.data(),frame_.size(),now);
            accept(now);
            frame_.clear(); expected_ = 0; started_ = true;
            continue;
        }
        frame_.push_back(byte);
        if (frame_.size() == 1 && (byte & 0xf0) != 0xa0) {
            fail(Error::Format, now); continue;
        }
        if (frame_.size() == 2) {
            expected_ = ((frame_[0] & 7u) << 8) | frame_[1];
            if (expected_ < 7 || expected_ > 2047) fail(Error::Length, now);
        }
    }
}
void Hdlc::accept(std::uint64_t now) {
    Frame frame;
    const auto status = validate_frame(frame_.data(), frame_.size(), frame);
    if (status != Error::None) { fail(status, now); return; }
    const bool iframe = (frame.control & 1) == 0;
    const bool ui = (frame.control & 0xef) == 3;
    if (frame.information.empty()) {
        // Ignore legal supervisory/link management frames, but do not let a link reset
        // join two unrelated segments. An empty I/UI frame is not a notification.
        if ((frame.control & 3) == 3 && assembling_) fail(Error::Sequence, now);
        return;
    }
    if (!iframe && !ui) { fail(Error::Control, now); return; }
    const auto sequence = static_cast<std::uint8_t>((frame.control >> 1) & 7);
    if (assembling_ && (iframe != segmented_iframe_ || frame.source != source_ || frame.destination != destination_ ||
                        (iframe && sequence != next_sequence_))) { fail(Error::Sequence, now); return; }
    if (frame.information.size() > max_apdu - assembled_.size()) { fail(Error::Overflow, now); return; }
    assembled_.insert(assembled_.end(), frame.information.begin(), frame.information.end());
    if (frame.segmented) {
        assembling_ = true; source_ = frame.source; destination_ = frame.destination;
        next_sequence_ = (sequence + 1) & 7; segmented_iframe_ = iframe; segment_time_ = now;
    } else {
        packet_(assembled_, now);
        assembled_.clear(); assembling_ = false;
    }
}
}
