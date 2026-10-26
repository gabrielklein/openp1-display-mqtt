#pragma once
#include "meter/protocol.hpp"
#include "input/pipeline.hpp"
#include "meter/presentation.hpp"
#include "display/view.hpp"
namespace openp1::network {
// Initialize source before worker tasks can expose a snapshot to MQTT.
bool start(input::Source source);
// Called only by acquisition owner. Never waits for a network operation.
void publish(const input::Pipeline& pipeline, std::uint64_t now);
bool copy_snapshot(meter::PresentationSnapshot& out,bool& station,bool& provisioning);
void decorate(display::View& view, std::uint64_t now);
}
