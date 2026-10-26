# MQTT

M4 implements bounded broker lifecycle/publication, certificate-verified TLS,
NVS configuration, retained LWT/availability and optional HA discovery/raw.
`config.hpp`, `json.hpp`, `payload.hpp`, `transport.hpp` contain testable builders;
`service.cpp` owns the client. No access-policy dependency. Replay is blocked.
See docs/M4.md for limits and live broker/TLS checks still pending.
