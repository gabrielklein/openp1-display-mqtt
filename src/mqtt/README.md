# MQTT

service.cpp owns broker/TLS/NTP lifecycle, bounded publication, status and
Discovery. Config/JSON/payload/transport/text helpers are host-testable. Real
publications use the root directly; replay appends /simulation. MQTT is independent
of web access policy. See ../../docs/MQTT.md and ../../docs/ARCHITECTURE.md.
