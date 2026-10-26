#include "protocol.hpp"
#include <algorithm>
#include <new>

namespace openp1::meter {
Processor::Processor() : hdlc_([this](const Bytes& data, std::uint64_t now) { decode(data, now, true); },
                              [this](Error e, std::uint64_t now) { error(e, now); }) {}
void Processor::error(Error e, std::uint64_t now) {
    ++counters_.errors; counters_.last_error = e; counters_.error_ms = now;
}
void Processor::reset() {
    hdlc_.reset(); direct_.clear(); readings_.clear(); document_ = {};
    layer_set_ = false; direct_failed_ = false; counters_ = {};
}
void Processor::discontinuity(std::uint64_t now) {
    hdlc_.reset(); direct_.clear(); direct_failed_ = true;
    error(Error::Discontinuity, now);
}
void Processor::feed(const std::uint8_t* data, std::size_t size, bool wire, std::uint64_t now) {
    if (!data || size == 0) return;
    if (layer_set_ && wire != wire_) {
        discontinuity(now); direct_failed_ = false;
    }
    wire_ = wire; layer_set_ = true;
    try {
    if (wire) hdlc_.feed(data, size, now);
    else if (!direct_failed_) {
        if (size > Hdlc::max_apdu - direct_.size()) {
            direct_.clear(); direct_failed_ = true; error(Error::Overflow, now);
        } else direct_.insert(direct_.end(), data, data + size);
    }
    }catch(const std::bad_alloc&){
        hdlc_.reset();direct_.clear();direct_failed_=true;error(Error::Memory,now);
    }
}
void Processor::finish(std::uint64_t now) {
    if (!layer_set_ || wire_) hdlc_.finish(now);
    else if (!direct_failed_ && !direct_.empty()) decode(direct_, now, false);
    direct_.clear(); direct_failed_ = false;
}
void Processor::decode(const Bytes& bytes, std::uint64_t now, bool validated) {
    try{decode_impl(bytes,now,validated);}
    catch(const std::bad_alloc&){error(Error::Memory,now);}
}
void Processor::decode_impl(const Bytes& bytes, std::uint64_t now, bool validated) {
    if(packet_)packet_(bytes,now);
    if (validated) {
        ++counters_.validated_hdlc;
        if (bytes.size() < 3 || bytes[0] != 0xe6 || bytes[1] != 0xe7 || bytes[2] != 0) {
            error(Error::Llc, now); return;
        }
    }
    const auto offset = bytes.size() >= 3 && bytes[0] == 0xe6 && bytes[1] == 0xe7 && bytes[2] == 0 ? 3u : 0u;
    if (bytes.size() <= offset || bytes[offset] != 0x0f) { error(Error::Apdu, now); return; }
    Document doc;
    if (!decode_notification(bytes, doc)) { error(Error::Axdr, now); return; }
    std::vector<Object> objects;
    if (!extract_objects(doc, objects)) { document_ = std::move(doc); error(Error::Objects, now); return; }
    auto next = readings_;
    for (auto& r : objects) {
        r.t_ms = now;
        const auto& v = doc.values[r.value_node];
        Sample sample{Bytes(doc.encoded.begin() + v.start, doc.encoded.begin() + v.start + v.size), r};
        const auto found = std::find_if(next.begin(), next.end(), [&](const Sample& s) {
            return s.reading.obis == r.obis && s.reading.attribute == r.attribute;
        });
        if (found != next.end()) {
            // Null/unavailable data is retained in the document, but cannot erase a good numeric sample.
            if (found->reading.numeric && !r.numeric) continue;
            *found = std::move(sample);
        }
        else if (next.size() < 64) next.push_back(std::move(sample));
        else { error(Error::Overflow, now); return; }
    }
    std::size_t retained_bytes = 0;
    for (const auto& sample : next) retained_bytes += sample.encoded_value.size();
    if (retained_bytes > Hdlc::max_apdu) { error(Error::Overflow, now); return; }
    readings_ = std::move(next); document_ = std::move(doc);
    ++counters_.notifications;
}
}
