# Input

Portable raw pipeline and replay: pipeline.hpp/cpp. ESP-IDF UART1 adapter:
uart.hpp/cpp. Both sources feed the same capture/processor pipeline.

Runtime simulation is configured in Full-access Settings, disabled by default
on fresh setup. Save simulation and restart persists its flag, sequence and
explicit wire-HDLC or reassembled LLC/APDU format, then applies the selection
after restart. No simulation build option remains. The default APDU preserves
the original capture except serial/GR identifiers 12345678 and 1234567.

The fixed-size simulation parser accepts hex lines and topic/with/slash hex
lines, bounded to 4096 text characters and 2048 decoded bytes. The scheduler
feeds 64-byte chunks at 20 ms intervals and repeats 3.5 seconds after each
cycle. Timeouts and display/network snapshots continue during the pause.
UART is not installed in simulation. MQTT uses the simulation branch and
distinct Home Assistant identities when enabled. See simulation/service.cpp
for NVS and delayed-restart ownership. File import/export, pause and speed
controls are outside the agreed scope.
