#pragma once
#include "config.hpp"
#include "input/pipeline.hpp"
namespace openp1::mqtt {
bool start();
bool get(Config& config); // Internal only; HTTP must sanitize secrets.
bool configure(const Config& config);
const char* status();
enum class Failure : unsigned {None,Connection,Publish,Start,Memory};
struct Statistics {
    bool connected=false;
    std::uint32_t sent=0,publish_failures=0,errors=0;
    Failure last_error=Failure::None;
    int error_type=0,broker_code=0,tls_code=0,tls_stack_code=0,socket_code=0;
};
Statistics statistics(); // Boot-lifetime counters; no payload/credential history.
const char* error_name(Failure error);
// Acquisition observers: nonblocking, exact observed frame bodies / LLC-APDU bytes.
void wire(const std::uint8_t* body,std::size_t size,input::Source source);
void apdu(const meter::Bytes& bytes,input::Source source);
std::uint32_t text_dropped();
}
