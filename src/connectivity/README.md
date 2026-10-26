# Connectivity

M3 implements AP/STA Wi-Fi, NVS persistence after successful provisioning,
bounded captive DNS and reconnect backoff. `network.cpp` owns lifecycle;
`model.hpp` contains portable guards, bounds and DNS logic. See docs/M3.md.

M4 adds guarded policy/grant state, optional GPIO35 S1 handling and a bounded
raw tail. MQTT/SSE consumers take independent snapshots; acquisition never
waits for network operations. See docs/M4.md.
