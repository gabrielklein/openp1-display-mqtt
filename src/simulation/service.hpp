#pragma once
#include "input/simulation.hpp"
namespace openp1::simulation {
// NVS must be initialized first. No tasks are started here.
bool start();
bool get(input::SimulationConfig& config);
const input::SimulationData& boot_data();
bool configure(const input::SimulationConfig& config,char* error,std::size_t capacity);
// Shared restart scheduler: call after a committed change and reply attempt.
// Available independently of simulation-settings initialization.
void request_restart();
bool restart_due(std::uint64_t now);
}
