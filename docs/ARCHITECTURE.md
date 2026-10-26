# Current architecture

## Data path and ownership

```text
UART or replay → bounded raw tail → HDLC validation/reassembly → DLMS A-XDR/OBIS
                                                        ↓
                                               last-good readings
                                                        ↓
                                          LCD / HTTP snapshots / MQTT
```

LLC/APDU replay enters explicitly after HDLC framing; it never becomes a validated
wire frame. The portable `src/meter/` parser preserves exact encoded values,
unknown objects, scalers/units and timestamps. It commits successful readings
without replacing valid data on failed input. Source bytes and reassembled payloads
remain distinct. All timeouts use monotonic time.

| Component | Current responsibility |
| --- | --- |
| `src/main.cpp` | Startup, selected acquisition source, parser/display/network updates, OTA startup tick |
| `src/input/` | Portable pipeline, bounded replay parser/scheduler, ESP-IDF UART adapter |
| `src/meter/` | Hex, HDLC CRC/segmentation, A-XDR/OBIS and presentation |
| `src/config/serial.*` | Versioned NVS UART settings; immutable boot profile |
| `src/simulation/` | NVS simulation selection/sequence and delayed restart |
| `src/access/` | Portable policies, timed grants and button state machine |
| `src/connectivity/` | AP/STA, captive DNS, reconnect worker, shared guarded state and S2 |
| `src/http/` | Offline pages, read APIs, guarded settings, SSE and OTA upload |
| `src/mqtt/` | Broker/TLS/NTP lifecycle, bounded publication, discovery and status |
| `src/display/` | Portable view/raster, ST7789 worker, display settings |
| `src/firmware/` | OTA target/header/date validation and startup confirmation |
| `web/` | Embedded setup, dashboard, settings, diagnostics and access-waiting pages |

`src/app/` and `src/diagnostics/` are directory placeholders, not separate runtime
services. Their current functionality lives in main/input/network/http/web.
The reserved storage partition remains unmounted; there is no capture filesystem.

## Bounds and memory

- Acquisition owns mutable parser state; readers take bounded snapshots without
  blocking acquisition on broker/browser I/O.
- Presentation: at most 64 readings and 8192 encoded-value bytes. Raw diagnostics:
  four records of at most 256 bytes each; separate snapshots, not full frame history.
- Display worker owns SPI and a one-item latest-view mailbox. RGB565 rendering uses
  a 240×8 DMA band (3840 bytes), never a 240×135 full DMA framebuffer.
- MQTT has a bounded text-frame queue and an 8192-byte QoS outbox. Allocation
  failure tears down the client and delays recreation for 15 seconds.
- Fallible large allocations use checked RAII helpers. C++ exceptions and a
  minimum 2048-byte emergency pool are required by the current build profile.
- SSE supports at most two async clients; each chunk rechecks View access.
- Status reads avoid allocating the large reading snapshot. Settings serializes
  requests and applies deadlines; mutating requests are never retried automatically.

## Configuration and access

Versioned NVS records preserve configuration across restart. UART and simulation
settings are applied at boot; display changes apply at refresh. Access-policy
saves persist then schedule restart. The network worker owns S2 and delayed reboot.

Full/origin/CSRF checks protect settings; ongoing upload/stream checks enforce
policy expiry. Setup is a separate Wi-Fi-only surface. TLS verifies trust chain
and hostname, with NTP readiness when needed. API responses expose configured
flags rather than usernames/passwords/certificates. There is no application login.

## MQTT and display semantics

UART publications use the saved root directly; replay appends `/simulation` and
uses separate Home Assistant identities. Every output, availability and LWT uses
that selected root. Leading `/` is preserved. State, summaries, hex and OBIS have
independent controls; new discovery is suppressed while state is disabled.

Sent count means accepted by the MQTT client, not proven broker/subscriber delivery.
Counters are boot-lifetime 32-bit values, individually sampled and wrap at 2^32.
Received count means successful decoded notifications, not segments or UART chunks.

Net power is scaled import minus export, red positive/green negative. Missing
export may be assumed zero for the derived web/LCD view only. Stale/invalid
transmitted values never become zero or overwrite parser/MQTT readings.

## OTA

The factory/NVS addresses remain fixed. `partitions.csv` defines factory plus
two OTA slots and metadata on the 16 MiB layout. Upload uses a 1024-byte stream
buffer and 288-byte prevalidation prefix, incremental erase, a 3 MiB/180-second
limit, repeated access checks and RAII abort. ESP-IDF validation precedes boot
selection; a selected pending restart blocks another upload. No full image is
held in RAM. Hash integrity does not authenticate a publisher.

An OTA image is confirmed after 30 seconds only when network/HTTP initialization
succeeded and acquisition reaches its loop. Failed initialization requests rollback;
reset before confirmation invokes bootloader rollback. Later arbitrary bugs are
not automatically detected. See FIRMWARE_UPDATE.md for operation and limitations.
