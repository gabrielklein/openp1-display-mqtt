# Current requirements

These requirements describe the current software. Historical notes under
`pre-release/` do not override them. Application text and documentation are English.

## Platform and acquisition

- PlatformIO, pinned espressif32 6.9.0 / ESP-IDF 5.3.1, C++17; no Arduino.
- Proposed Tenstar TS0636G classic ESP32 T-Display, 16 MiB flash.
- One shared portable pipeline for UART and replay; never acquire both together.
- UART1 GPIO27, configurable baud/parity/stop bits/inversion. Default 115200 8N1
  inverted. Save to NVS and restart; electrical compatibility remains unverified.
- Preserve exact incoming bytes and unknown objects. Parse HDLC declared lengths,
  validate checksums, reassemble segments, then decode DLMS A-XDR/OBIS/scalers/units.
  Failed input cannot overwrite last-good values with zeros.
- Bound all buffers and uploads. Keep acquisition independent of network waits.

## Network and access

- Unconfigured: AP/captive DNS and Wi-Fi-only provisioning with scan/manual SSID.
  Configured: station mode with reconnect backoff; outages do not erase credentials.
- Persist None/View/Full, default Full. No application password. None blocks
  readings/configuration but keeps MQTT/acquisition/LCD running. View is read-only.
- Enforce access server-side on every request and ongoing stream/upload.
- Optional active-low GPIO35 S1: short release grants Full for 300 seconds;
  eight-second hold resets Wi-Fi only. Reboot clears temporary access.
- S1 is disabled by default until its external/onboard pull-up is verified.
- Locked navigation uses an S1 waiting page. Its empty Full-access probe is the
  explicit public exception; it exposes no readings, secrets or configuration.
- Mutations require Full, station origin/Host validation and CSRF. Setup AP must
  not expose configuration, diagnostics or updates beyond Wi-Fi provisioning.

## Display, web and diagnostics

- Color LCD: large import/export power, three phase voltages, Wi-Fi/MQTT state,
  receive pulse and IP/access footer. Persistent optional 180° rotation.
- Offline responsive dashboard: net-power gauge above Wi-Fi, then connection/source;
  timestamps, stale state and decoded-message count. Gauge defaults to ±5 kW.
- Missing export may mean zero only for the derived gauge/LCD. Missing import or
  present invalid/stale export must not become zero. Never alter decoded values.
- Settings: uptime, version, runtime UART/simulation/display/access/MQTT configuration,
  broker errors and accepted-publication count. No secrets in API reads/logs.
- Read-only View/Full diagnostics: bounded raw chunks, decoded objects and latest
  error/counters. No full history, export/import/copy/download or MQTT debug console.

## MQTT and replay

- Plain MQTT default port 1883; optional certificate/hostname-verified TLS.
  Never offer insecure TLS. Persist configuration without returning credentials.
- Real data: `<prefix>/<topic>`; replay: `<prefix>/simulation/<topic>`.
  Default prefix `openp1/<stable-device-id>`; preserve an optional leading `/`.
- Independent state JSON, text summary, raw/HDLC hex and individual OBIS switches.
  Retained availability/LWT and optional retained Home Assistant Discovery.
  Discovery needs state output; disabling it does not delete retained entities.
- Omit missing/stale/incompatible summary groups; apply transmitted scalers to OBIS.
- Replay defaults disabled; explicit wire-HDLC or LLC/APDU input. Max 4096 text
  characters / 2048 decoded bytes, fixed 3.5-second repeat. No fabricated HDLC.
  Built-in and test meter identifiers are 12345678/1234567.

## Firmware updates and validation

- Full-only Settings application `.bin` upload, 3 MiB/180-second maximum, bounded
  streaming into an inactive OTA slot; check access through final activation.
- Validate chip/project/date/image integrity before boot selection. Abort failures;
  preserve NVS. Enable startup confirmation and bootloader rollback.
- Version scheme YYYY.MM.DD; build script packages application/checksum/metadata
  without flashing. Old factory-only installations require one USB migration.
- M0–M6 functionality is implemented and accepted; M7 physical/electrical, soak,
  enclosure, clean installation and release audit remain. See TEST_PLAN.md.
- Never power from meter and USB together. Never flash without user instruction.
