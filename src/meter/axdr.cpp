#include "protocol.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace openp1::meter {
namespace {
class Reader {
public:
    explicit Reader(Document& doc,bool inspect=false) : doc_(doc),inspect_(inspect) {}
    std::size_t nodes=0;
    std::size_t pos = 0;
    bool length(std::size_t& count) {
        if (pos >= doc_.encoded.size()) return false;
        count = doc_.encoded[pos++];
        if (count < 128) return true;
        const auto octets = count & 127;
        if (octets == 0 || octets > 2 || octets > doc_.encoded.size() - pos) return false;
        if (doc_.encoded[pos] == 0) return false;
        count = 0;
        for (std::size_t i = 0; i < octets; ++i) count = (count << 8) | doc_.encoded[pos++];
        return count >= 128;
    }
    bool value(std::size_t& index, unsigned depth) {
        if (depth > 12 || nodes >= 256 || pos >= doc_.encoded.size()) return false;
        index = nodes++;
        if(!inspect_)doc_.values.emplace_back();
        Value node;
        node.start = pos; node.tag = doc_.encoded[pos++];
        std::size_t length_bytes = 0;
        switch (node.tag) {
        case 1: case 2: {
            std::size_t count = 0;
            if (!length(count) || count > 128) return false;
            if(!inspect_)node.children.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                std::size_t child = 0;
                if (!value(child, depth + 1)) return false;
                if(!inspect_)node.children.push_back(child);
            }
            break;
        }
        case 0: break;
        case 3: case 13: case 15: case 17: case 22: length_bytes = 1; break;
        case 16: case 18: length_bytes = 2; break;
        case 5: case 6: case 23: case 27: length_bytes = 4; break;
        case 20: case 21: case 24: length_bytes = 8; break;
        case 25: length_bytes = 12; break;
        case 26: length_bytes = 5; break;
        case 4: {
            std::size_t bits = 0;
            if (!length(bits) || bits > 8192) return false;
            length_bytes = (bits + 7) / 8; break;
        }
        case 9: case 10: case 12:
            if (!length(length_bytes) || length_bytes > 1024) return false;
            break;
        default: return false; // Unknown wire types cannot safely be skipped.
        }
        node.payload = pos;
        node.length = length_bytes;
        if (length_bytes > doc_.encoded.size() - pos) return false;
        if (node.tag == 3 && doc_.encoded[pos] > 1) return false;
        pos += length_bytes;
        node.size = pos - node.start;
        if(!inspect_)doc_.values[index] = std::move(node);
        return true;
    }
private:
    Document& doc_;
    bool inspect_=false;
};
bool octets(const Document& d, std::size_t index, std::size_t size) {
    return d.values[index].tag == 9 && d.values[index].length == size;
}
bool unsigned_integer(const Document& d, std::size_t index, std::uint64_t& result) {
    const auto& v = d.values[index];
    if (v.tag != 6 && v.tag != 17 && v.tag != 18 && v.tag != 21 && v.tag != 22) return false;
    result = 0;
    for (std::size_t i = 0; i < v.length; ++i) result = (result << 8) | d.encoded[v.payload + i];
    return true;
}
bool scaler_unit(const Document& d, std::size_t index, std::int8_t& scaler, std::uint8_t& unit) {
    const auto& v = d.values[index];
    if (v.tag != 2 || v.children.size() != 2) return false;
    const auto& s = d.values[v.children[0]];
    const auto& u = d.values[v.children[1]];
    if (s.tag != 15 || u.tag != 22) return false;
    const auto raw = d.encoded[s.payload];
    scaler = static_cast<std::int8_t>(raw < 128 ? raw : static_cast<int>(raw) - 256);
    unit = d.encoded[u.payload]; return true;
}
Object object(const Document& d, std::size_t obis, std::size_t value) {
    Object r;
    std::copy_n(d.encoded.begin() + d.values[obis].payload, 6, r.obis.begin());
    r.type = d.values[value].tag; r.value_node = value;
    r.numeric = number(d, value, r.raw_number);
    r.scaled = r.raw_number;
    return r;
}
}
bool decode_notification(const Bytes& apdu, Document& output) {
    if (apdu.empty() || apdu.size() > Hdlc::max_apdu) return false;
    Document doc;
    doc.encoded = apdu;
    Reader reader(doc,true);
    // Input may be an LLC+APDU or a bare APDU; neither is an HDLC frame.
    if (apdu.size() >= 3 && apdu[0] == 0xe6 && apdu[1] == 0xe7 && apdu[2] == 0) reader.pos = 3;
    if (apdu.size() - reader.pos < 6 || apdu[reader.pos++] != 0x0f) return false;
    for (unsigned i = 0; i < 4; ++i) doc.invoke_id = (doc.invoke_id << 8) | apdu[reader.pos++];
    std::size_t datetime_size = 0;
    if (!reader.length(datetime_size) || (datetime_size != 0 && datetime_size != 12) ||
        datetime_size > apdu.size() - reader.pos) return false;
    doc.datetime.assign(apdu.begin() + reader.pos, apdu.begin() + reader.pos + datetime_size);
    reader.pos += datetime_size;
    const auto root_start=reader.pos;
    if (!reader.value(doc.root, 0) || reader.pos != apdu.size()) return false;
    doc.values.reserve(reader.nodes); // One exact node allocation, no growth peak.
    Reader build(doc);build.pos=root_start;
    if(!build.value(doc.root,0)||build.pos!=apdu.size())return false;
    output = std::move(doc);
    return true;
}
bool number(const Document& doc, std::size_t index, double& output) {
    if (index >= doc.values.size()) return false;
    const auto& v = doc.values[index];
    switch (v.tag) {
    case 5: case 6: case 15: case 16: case 17: case 18: case 20: case 21: case 22: case 23: case 24: break;
    default: return false;
    }
    std::uint64_t raw = 0;
    for (std::size_t i = 0; i < v.length; ++i) raw = (raw << 8) | doc.encoded[v.payload + i];
    if (v.tag == 23) {
        const auto bits = static_cast<std::uint32_t>(raw);
        float value;
        static_assert(sizeof(value) == sizeof(bits), "IEEE float required");
        std::memcpy(&value, &bits, sizeof(value)); output = value;
    } else if (v.tag == 24) {
        static_assert(sizeof(output) == sizeof(raw), "IEEE double required");
        std::memcpy(&output, &raw, sizeof(raw));
    } else if ((v.tag == 5 || v.tag == 15 || v.tag == 16 || v.tag == 20) &&
               (raw & (std::uint64_t{1} << (v.length * 8 - 1)))) {
        const auto mask = v.length == 8 ? UINT64_MAX : (std::uint64_t{1} << (v.length * 8)) - 1;
        output = -1.0 - static_cast<double>((~raw) & mask);
    } else output = static_cast<double>(raw);
    return std::isfinite(output);
}
const char* unit_name(std::uint8_t unit) {
    switch (unit) {
    case 27: return "W"; case 28: return "VA"; case 29: return "VAR";
    case 30: return "WH"; case 31: return "VAH"; case 32: return "VARH";
    case 33: return "A"; case 35: return "V"; case 44: return "HZ";
    case 255: return ""; default: return nullptr;
    }
}
const char* known_name(const std::array<std::uint8_t, 6>& o) {
    if (o[0] != 1 || o[1] != 0 || o[5] != 255) return nullptr;
    if (o[3] == 7 && o[4] == 0) {
        switch (o[2]) {
        case 1: return "Import"; case 2: return "Export";
        case 21: return "Import L1"; case 22: return "Export L1";
        case 41: return "Import L2"; case 42: return "Export L2";
        case 61: return "Import L3"; case 62: return "Export L3";
        case 32: return "Voltage L1"; case 52: return "Voltage L2"; case 72: return "Voltage L3";
        case 31: return "Current L1"; case 51: return "Current L2"; case 71: return "Current L3";
        }
    }
    if (o[3] == 8 && o[4] == 0) {
        if (o[2] == 1) return "Import energy";
        if (o[2] == 2) return "Export energy";
    }
    return nullptr;
}
bool extract_objects(const Document& d, std::vector<Object>& output) {
    if (d.root >= d.values.size()) return false;
    std::vector<Object> objects;
    const auto& root = d.values[d.root];
    if (root.tag != 1 && root.tag != 2) { output.clear(); return true; }
    // Self-describing tuple list, as seen in the supplied ISK capture.
    bool tuples = !root.children.empty();
    for (auto entry : root.children) {
        const auto& v = d.values[entry];
        if (v.tag != 2 || (v.children.size() != 2 && v.children.size() != 3) ||
            !octets(d, v.children[0], 6)) { tuples = false; break; }
    }
    if (tuples) {
        for (auto entry : root.children) {
            const auto& v = d.values[entry];
            auto r = object(d, v.children[0], v.children[1]);
            if (v.children.size() == 3) {
                if (!scaler_unit(d, v.children[2], r.scaler, r.unit)) return false;
                r.has_scaler_unit = true;
            }
            objects.push_back(r);
        }
    } else if (!root.children.empty() && d.values[root.children[0]].tag == 1) {
        // Standard push object list (class, logical name, attribute, data index).
        const auto& list = d.values[root.children[0]];
        const bool includes_self = root.children.size() == list.children.size();
        if (!includes_self && root.children.size() != list.children.size() + 1) return false;
        if (includes_self) {
            if (list.children.empty()) return false;
            const auto& self = d.values[list.children[0]];
            if (self.tag != 2 || self.children.size() != 4) return false;
            const auto& c = self.children;
            std::uint64_t class_id = 0, data_index = 0;
            if (!unsigned_integer(d, c[0], class_id) || class_id != 40 ||
                !octets(d, c[1], 6) || d.values[c[2]].tag != 15 ||
                d.encoded[d.values[c[2]].payload] != 2 ||
                !unsigned_integer(d, c[3], data_index) || data_index != 0) return false;
        }
        const auto value_offset = includes_self ? 0u : 1u;
        for (std::size_t i = 0; i < list.children.size(); ++i) {
            const auto& descriptor = d.values[list.children[i]];
            if (descriptor.tag != 2 || descriptor.children.size() != 4) return false;
            const auto& c = descriptor.children;
            std::uint64_t class_id = 0, data_index = 0;
            if (d.values[c[0]].tag != 18 || d.values[c[3]].tag != 18 ||
                !unsigned_integer(d, c[0], class_id) || class_id > 65535 ||
                !octets(d, c[1], 6) || d.values[c[2]].tag != 15 ||
                !unsigned_integer(d, c[3], data_index)) return false;
            for (std::size_t j = 0; j < i; ++j) {
                const auto& previous = d.values[list.children[j]].children;
                std::uint64_t previous_class = 0, previous_index = 0;
                if (!unsigned_integer(d, previous[0], previous_class) ||
                    !unsigned_integer(d, previous[3], previous_index)) return false;
                if (class_id == previous_class && data_index == previous_index &&
                    d.encoded[d.values[c[2]].payload] == d.encoded[d.values[previous[2]].payload] &&
                    std::equal(d.encoded.begin() + d.values[c[1]].payload,
                               d.encoded.begin() + d.values[c[1]].payload + 6,
                               d.encoded.begin() + d.values[previous[1]].payload)) return false;
            }
            // Partial attribute element selection is preserved in the tree, not mislabelled.
            if (data_index != 0 || (includes_self && i == 0)) continue;
            auto r = object(d, c[1], root.children[i + value_offset]);
            r.class_id = static_cast<std::uint16_t>(class_id);
            r.attribute = d.encoded[d.values[c[2]].payload];
            if (r.attribute == 2) objects.push_back(r);
        }
        for (auto& r : objects) {
            if (r.class_id != 3 && r.class_id != 4) continue;
            for (std::size_t i = 0; i < list.children.size(); ++i) {
                const auto& c = d.values[list.children[i]].children;
                if (d.encoded[d.values[c[2]].payload] != 3 ||
                    !std::equal(r.obis.begin(), r.obis.end(), d.encoded.begin() + d.values[c[1]].payload)) continue;
                std::uint64_t class_id = 0, data_index = 0;
                if (!unsigned_integer(d, c[0], class_id) || class_id != r.class_id ||
                    !unsigned_integer(d, c[3], data_index) || data_index != 0) continue;
                if (!scaler_unit(d, root.children[i + value_offset], r.scaler, r.unit)) return false;
                r.has_scaler_unit = true;
            }
        }
    } // Unmapped valid A-XDR is retained as Document, without invented readings.
    if (objects.size() > 64) return false;
    for (auto& r : objects) {
        if ((r.type == 5 || r.type == 6 || (r.type >= 15 && r.type <= 18) ||
             (r.type >= 20 && r.type <= 24)) && !r.numeric) return false;
        if (r.numeric && r.has_scaler_unit) {
            r.scaled = r.raw_number * std::pow(10.0, r.scaler);
            if (!std::isfinite(r.scaled)) return false;
        }
    }
    // Duplicate identifiers/attributes are ambiguous: reject the entire update.
    for (std::size_t i = 0; i < objects.size(); ++i)
        for (std::size_t j = i + 1; j < objects.size(); ++j)
            if (objects[i].obis == objects[j].obis && objects[i].attribute == objects[j].attribute) return false;
    output = std::move(objects);
    return true;
}
}
