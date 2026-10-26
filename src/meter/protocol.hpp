#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <utility>

namespace openp1::meter {
using Bytes = std::vector<std::uint8_t>;
// CRC-16/X-25, reflected 0x1021, init/xorout 0xffff; transmitted low byte first.
std::uint16_t crc16(const std::uint8_t* data, std::size_t size);
enum class Error { None, Format, Length, Hcs, Fcs, Address, Control, Sequence,
                   Overflow, Timeout, Truncated, Llc, Apdu, Axdr, Objects, Discontinuity, Memory };
const char* error_name(Error error);
struct Frame {
    std::uint32_t destination = 0, source = 0;
    std::uint8_t control = 0;
    bool segmented = false;
    Bytes information;
};
// Body includes format through FCS, excludes flags. No address/session assumptions.
Error validate_frame(const std::uint8_t* body, std::size_t size, Frame& frame);
class Hdlc {
public:
    static constexpr std::size_t max_apdu = 8192;
    using Packet = std::function<void(const Bytes&, std::uint64_t)>;
    using Failure = std::function<void(Error, std::uint64_t)>;
    using WireFrame=std::function<void(const std::uint8_t*,std::size_t,std::uint64_t)>;
    Hdlc(Packet packet, Failure failure);
    void observe(WireFrame observer){wire_frame_=std::move(observer);}
    void feed(const std::uint8_t* data, std::size_t size, std::uint64_t now);
    void tick(std::uint64_t now);
    void finish(std::uint64_t now);
    void reset();
private:
    void fail(Error error, std::uint64_t now);
    void accept(std::uint64_t now);
    WireFrame wire_frame_;
    Packet packet_;
    Failure failure_;
    Bytes frame_, assembled_;
    std::size_t expected_ = 0;
    bool started_ = false, assembling_ = false;
    std::uint32_t source_ = 0, destination_ = 0;
    std::uint8_t next_sequence_ = 0;
    bool segmented_iframe_ = false;
    std::uint64_t frame_time_ = 0, segment_time_ = 0;
};

// Flat tree: offsets reference Document::encoded, preserving every original type/value.
struct Value {
    std::uint8_t tag = 0;
    std::size_t start = 0, size = 0, payload = 0, length = 0;
    std::vector<std::size_t> children;
};
struct Document {
    Bytes encoded;
    std::vector<Value> values;
    std::size_t root = 0;
    Bytes datetime; // Original optional notification timestamp; not converted to local time.
    std::uint32_t invoke_id = 0;
};
bool decode_notification(const Bytes& apdu, Document& result);
bool number(const Document& doc, std::size_t index, double& output);
struct Reading {
    std::array<std::uint8_t, 6> obis{};
    std::uint16_t class_id = 0;
    std::uint8_t attribute = 2, type = 0;
    bool numeric = false, has_scaler_unit = false;
    double raw_number = 0, scaled = 0;
    std::int8_t scaler = 0;
    std::uint8_t unit = 0;
    std::uint64_t t_ms = 0;
};
struct Object : Reading {
    std::size_t value_node = 0; // References the document used for this extraction only.
};
const char* unit_name(std::uint8_t unit);
const char* known_name(const std::array<std::uint8_t, 6>& obis);
// Accepts explicit {OBIS,value[,scaler-unit]} entries, or a push object list
// followed by exactly its values. Never infers identifiers/scalers by position alone.
bool extract_objects(const Document& doc, std::vector<Object>& result);
struct Counters {
    std::uint64_t errors = 0, notifications = 0, validated_hdlc = 0;
    Error last_error = Error::None;
    std::uint64_t error_ms = 0;
};
struct Sample {
    Bytes encoded_value; // Exact original A-XDR value (including its type tag).
    Reading reading;
};
class Processor {
public:
    Processor();
    void observe(Hdlc::WireFrame frame,Hdlc::Packet packet){hdlc_.observe(std::move(frame));packet_=std::move(packet);}
    Processor(const Processor&) = delete;
    Processor& operator=(const Processor&) = delete;
    void feed(const std::uint8_t* data, std::size_t size, bool wire_hdlc, std::uint64_t now);
    void tick(std::uint64_t now) { hdlc_.tick(now); }
    void finish(std::uint64_t now);
    void discontinuity(std::uint64_t now);
    void reset();
    const std::vector<Sample>& readings() const { return readings_; }
    const Document& last_document() const { return document_; }
    const Counters& counters() const { return counters_; }
    // Explicit APDU-layer replay is buffered until finish(); no HDLC validation claim.
private:
    void decode(const Bytes& bytes, std::uint64_t now, bool validated);
    void decode_impl(const Bytes& bytes, std::uint64_t now, bool validated);
    void error(Error error, std::uint64_t now);
    Hdlc::Packet packet_;
    Hdlc hdlc_;
    Bytes direct_;
    bool layer_set_ = false, wire_ = true, direct_failed_ = false;
    Document document_;
    std::vector<Sample> readings_;
    Counters counters_;
};
}
