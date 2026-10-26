# Architecture and intended module boundaries

| Directory | Responsibility |
| --- | --- |
| src/app | Lifecycle and event coordination |
| src/config | Validated versioned settings, NVS, secret-safe reads |
| src/connectivity | Station/AP state machine and captive DNS |
| src/access | Persisted policy, temporary button grant, request guards |
| src/input | UART driver and replay reader producing byte chunks |
| src/meter | Portable framing, checksums, reassembly, DLMS A-XDR, OBIS |
| src/diagnostics | Bounded raw/event history and replayable export |
| src/display | ST7789 driver and display states |
| src/http | REST, static UI, live WebSocket, uploads |
| src/mqtt | Broker lifecycle, TLS, availability, discovery |
| web | HTML/CSS/JS embedded into firmware at build time |

Data path: selected source -> raw capture -> HDLC validation/reassembly -> DLMS decode -> immutable latest-reading snapshot -> display / HTTP / MQTT. LLC/APDU captures enter at a separately identified layer. Keep source byte chunks distinct from protocol frames.

Acquisition must not wait for browsers or a broker. Bounded FreeRTOS queues and explicit overflow counters; single owner per mutable state, safe snapshots for readers. Retain last good values with age/error status. Use monotonic timing for timeouts and replay even before network time is available.

M2 explicitly compiles main, meter/hex, meter/hdlc, meter/axdr, meter/processor, input/pipeline, input/uart, display/view and display/st7789 with driver, esp_timer and bundled esp_lcd dependencies. UART and replay feed the same single-owner bounded RAM history and portable processor. Reading updates commit atomically; each sample retains its own timestamp and exact value bytes. The latest parsed A-XDR document preserves unknown objects. The acquisition task sends fixed display views to a one-item latest-value queue; only the display task owns SPI and the DMA band buffer. The reserved SPIFFS partition remains unmounted: M1 needs no persistent storage or uploads; choose storage and enforce upload bounds in M5. See M1.md for the raw-input baseline and M2.md for current ownership, bounds, protocol support and display limitations.

## M3 implementation

`connectivity/network.cpp` owns Wi-Fi/NVS lifecycle, event callbacks, captive DNS
and the reconnect worker. `http/server.cpp` owns request guards, JSON responses
and setup submissions. `connectivity/internal.hpp` defines mutex-protected fixed
snapshot/state shared between them; acquisition publishes without waiting.
`web/pages.hpp` is compiled into firmware. HTTP uses polling, with no streaming
connection. M3 was accepted by user report; see M4.md for current acceptance still pending.

The original provisioning wizard schedules asynchronous radio scans on the
network worker, receives completion via the Wi-Fi event callback and reads
bounded cached results from HTTP. A guarded setup state exposes connection
progress and a ten-second success/IP handoff before AP closure. Acquisition
ownership and the UART/replay processor are unchanged. This uses only ESP-IDF;
no NetWizard implementation/assets or Arduino component was added.

`meter/presentation.hpp` provides portable OBIS formatting, tariff labels,
string rendering and a fixed presentation snapshot (64 readings, at most 8192
encoded value bytes). Acquisition publishes this snapshot without waiting;
HTTP allocates its bounded copy on the heap. Exact decoded values stay in the
processor, and strings do not become numeric readings on the physical display.

## M4 implementation

`access/control.hpp` holds the portable grant/debounce/long-press rules; network
state mutex protects policy, grant and button countdown. The network worker
owns GPIO35 and Wi-Fi-only reset. Acquisition publishes bounded presentation
and four-record raw snapshots without waiting; HTTP reads separate copies.
`http/admin.cpp` validates Full/CSRF and bounded settings, while `http/stream.cpp`
owns at most two async SSE requests and checks access before every chunk.
`mqtt/service.cpp` owns the broker client, bounded text-frame queue, config/NTP lifecycle
and publication. Its mutex/atomics are independent of access. The MQTT task takes
read-only snapshot copies; TLS uses ESP-TLS with chain and hostname verification.
Portable config/payload/transport builders have host tests. See M4.md for limits
and the physical/live acceptance still required. Legacy include/settings.hpp
remains the serial scaffold; runtime MQTT uses versioned mqtt::Config in NVS.

## MQTT status counters

The debug console, payload history and `/api/v1/debug` routes are removed by user
request. Settings polls Full-access, station-origin-guarded `/api/v1/mqtt/status`
every five seconds through the serialized request queue. It copies only atomic
counters, never the broker config or a reading snapshot. MQTT exposes connection
state, boot-lifetime accepted-publication and failure/error counts, plus the last
error category and numeric broker/TLS/socket details. Reconnection preserves the
last error for diagnosis; non-connection failures clear old connection codes.
Fields are individually atomic and may span concurrent events. Counters are
32-bit and wrap after 2^32 events; restart clears them. All configured source
branches, discovery and availability count; broker-generated LWT does not.
Client acceptance does not guarantee delivery to a subscriber.

The dashboard displays `notifications`: successfully decoded meter messages
since boot, including simulation, rather than UART chunks, HDLC segments or MQTT
subscriptions. Failed decodes never increment this count. Existing raw/protocol
diagnostics remain available under their existing access policies.

Large fallible allocations in HTTP and MQTT use `support/memory.hpp`: checked
`malloc` plus placement construction and matching destruction/free, including
request bodies. This avoids the pinned toolchain's `new(std::nothrow)` path,
which can call the throwing allocator and abort on failure with C++ exceptions
disabled. Existing HTTP/SSE allocation guards can now return unavailable or
close the stream, and MQTT startup/worker allocation failure can remain an error
without aborting. The memory-pressure follow-up below also enables guarded
C++ allocation exceptions. Physical soak-test acceptance remains pending.

## Normal and simulation MQTT channels (2026-10-25)

The user's explicit request to publish simulation supersedes the earlier replay
publication block. MQTT's enable setting now applies to both sources. The saved
prefix remains unchanged in NVS; the service appends only `/simulation` for replay and no suffix for UART
before `/state` and `/availability`, even for a custom prefix. State JSON
retains its `simulation` boolean. Text frames use the same isolated branch.
The normal Home Assistant identity remains stable so its retained discovery
configuration updates the topic references. Simulation uses a distinct client,
device and entity identity with `_simulation`, plus visible simulation names.

The application selects its input source and initializes the network snapshot
with that source before starting any worker tasks, preventing normal-channel publication during
simulation startup. On a source change the MQTT task disables text admission,
sends offline on the old branch when possible, stops/destroys the client, clears
the text-frame queue, selects the new branch/LWT and recreates the client. Admission
checks source, and draining checks it again to reject a late old-source enqueue.
Reconnection reannounces only the active branch. Wi-Fi setup/connection gates,
TLS verification, access independence and memory limits remain in effect.
The runtime simulation settings below subsequently add an explicit source selection.

## Runtime simulation settings (2026-10-25)

`input/simulation.hpp/cpp` holds the portable, fixed-size configuration/parser
and scheduler. The default capture preserves the supplied APDU except for two
same-length identifier replacements. `prepare_simulation` validates text syntax
without dynamic allocation, yielding at most 2048 exact bytes. It never infers
a wire envelope, computes a CRC or treats malformed protocol data as validated.
The player feeds the same `Pipeline::ingest`/Processor as UART: 64-byte chunks
at 20 ms intervals, then a 3500 ms pause from the final chunk.

`simulation/service.cpp` owns versioned NVS configuration, its mutex and an
immutable prepared boot sequence. Initialize NVS and simulation settings before
selecting the source and starting network/MQTT. Missing settings default to
UART; malformed persisted simulation settings fall back to the disabled default
without erasing NVS, allowing repair through Settings. Both input paths are
always compiled; simulation Kconfig options and injected build text are removed.

GET/PUT `/api/v1/simulation` require Full access and station-origin validation;
PUT also requires CSRF, at most 8192 request bytes, strict types/known unique
fields, an explicit known layer and valid hex input. Partial disable preserves
the saved sequence. A successful NVS commit updates the saved configuration;
the running boot sequence stays immutable. Only after a successful HTTP save
response is sent is a restart scheduled 1500 ms later on the network worker.
This resets UART/parser/client ownership cleanly and applies the chosen source
before any new MQTT publication. Restart timing handles 32-bit millisecond wrap.
The UI labels the restart, provides a default-sequence button and displays input
validation failures. File upload/export, pause/speed and other M5 controls are
still unimplemented. Physical restart and UART-return acceptance remains pending.

## MQTT text compatibility output (2026-10-25)

The text format lives under `<prefix>/<mode>/`; the former MQTT `/raw`
JSON topic and its queue/producer/API option are removed. State JSON and text use one client and
the configured broker/TLS/credentials/prefix; normal and simulation remain
separate. MQTT Config v2 appends `text_enabled` (default true) and printable
32-character `text_id` (default user label ISK). A version-1 3632-byte NVS prefix
is migrated in RAM, preserving all existing settings/secrets, then saved as v2
on the next configuration write. A blank label omits `text/id`. Full-access and
CSRF guards for MQTT settings remain in effect.

`mqtt/text.hpp` formats matched OBIS attribute-2 readings only: active power
W and total kWh; mean phase voltage then L1/L2/L3; summed phase current then
L1/L2/L3; tariff import 1/2 and export 1/2 in Wh. It checks numeric/finite values,
transmitted scalers/expected units, matching sample times and age below 15 s.
Missing/incompatible/stale groups are omitted, never zero-filled. Derived mean
and sum exist only in the compatibility topic, not in the reading snapshot.
Summary publication follows the existing state cadence (new notification or
five seconds); text messages are non-retained QoS 0.

Optional acquisition observers receive frame bodies only at the declared closing
flag in the shared HDLC framer, before CRC validation, and original LLC/APDU
bytes at decode entry after HDLC validation/reassembly or explicit direct replay.
They never scan for embedded flags or synthesize a wire frame from an APDU.
`mqtt::wire` copies the observed body with its observed flag boundaries into
a four-slot queue of at most 2049 bytes per full frame. CRC-invalid complete
frames can still be shown as raw. Malformed/truncated data without complete
boundaries remains in the existing raw chunk history/JSON. `mqtt::apdu` uses a
nonblocking single pending 2048-byte mailbox; overwrites/contention/oversize are
counted. Direct LLC/APDU replay produces only `hdlc`, never `raw`.

Text-raw resources are allocated lazily when the text_raw_enabled setting is
enabled, then retained for the boot lifetime to avoid freeing producer-visible
queues. The MQTT worker drains/rechecks the source and converts one message
at a time using a checked hex allocation of at most 4099 bytes. It can hold the
packet mutex across publication; acquisition only tries it with zero wait and
counts a skip. `text_dropped` reports resource failures, queue/mailbox loss,
oversize payloads and publish failures, without retaining their contents.
HTTP raw diagnostics, access-independent acquisition and the bounded meter
processor remain unchanged. Physical broker integration remains unverified.

## Memory-pressure recovery (2026-10-25)

User ELF `38f30d39e…` resolves to allocator failure in A-XDR node growth and
MQTT state JSON key creation. The profile now deliberately enables C++
exceptions with a 2048-byte emergency pool. Startup warms the exception runtime
before Wi-Fi, matching the pinned IDF emergency-pool test. Compile-time guards
reject disabled exceptions or an insufficient pool. Checked C allocation remains
for explicitly fallible buffers.

A-XDR validates once without node/child allocations, then reserves the exact
sizes for the real parse. Processor feed/decode catch bad_alloc, record
`memory exhausted`, discard partial assembly as needed and preserve the last
committed readings/document; Pipeline has already retained the raw input.
MQTT uses stack OBIS keys, fixed discovery-cache metadata and RAII-owned JSON
trees, rejecting incomplete serialization. It constructs topics before JSON
ownership; allocation failure stops the client and retries after one second.
Packet mutexes unwind safely; startup buffer allocation is retried. HTTP
callbacks return unavailable and SSE closes affected streams on allocation failure.

The debug-history allocation was later removed with the console. Dynamic Wi-Fi RX/TX caps
are 10/8 (RX meets static RX count 10), reducing burst memory/peak throughput.
The build reports static RAM 68632 bytes versus 88736 previously; this is not
runtime headroom. Startup logs report free/largest 8-bit heap; guarded status
APIs also report minimum-free watermark. Real-device stability and radio
performance remain unverified; this is not recovery from arbitrary SDK/hardware faults.

MQTT Config v3 reuses the former reserved layout byte for text_raw_enabled,
without changing the v1 prefix/v2 size. Migrate v2 by assigning text_raw_enabled
from the old text_enabled, preserving both groups' prior behavior. V1 migration
enables both, matching the former text default. The next save writes v3.
text_enabled controls ID/measurement summaries only; text_raw_enabled controls
frame/APDU observers, resources and hex publication independently. Incoming `raw`
JSON configuration fields remain rejected. Broker settings/secrets are preserved.

## Direct compatibility and OBIS topics (2026-10-25)

The `/text/` namespace has been removed. Compatibility and hex topics now live
immediately below the configured prefix and normal/simulation branch. `/raw`
now carries hexadecimal frame bytes only; its former JSON chunk publisher stays
removed. No duplicate publications are sent to old `/text/` topics.

Config v4 adds default-off `obis_enabled` in former trailing padding, retaining
the existing NVS size and prefix. V1/v2/v3 migrate to v4 with OBIS disabled;
independent text/hex flags and broker credentials are preserved. The API and
Settings expose a strict boolean independent of summary/hex output.

`obis_payload` emits attribute-2 values fresher than 15 seconds: finite scaled
numbers (or raw numeric values without a scaler), decoded text or hexadecimal
binary strings, bounded to 512 payload bytes. Malformed/oversize values are
omitted. The worker uses existing display-OBIS formatting, including `*F` when
F differs from 255, so e.g. `1-0:51.7.0` is not an invented meter object. All
snapshot OBIS values remain present in state JSON; optional per-OBIS output uses
the same notification/five-second cadence, QoS 0 and no retain. Counters include
these publications. Unknown OBIS codes are allowed if their value is supported.

## Optional JSON state publication (2026-10-25)

Config v5 adds default-on state_enabled in the last trailing padding byte,
keeping the v2 NVS size/v1 prefix. V1-v4 migrate with state enabled and preserve
other controls/credentials. The MQTT worker skips both state JSON construction
and publication while disabled; summary, hex, OBIS and availability keep their
independent controls. HTTP/SSE serialization is unaffected. New discovery is
suppressed while state is disabled because its entities refer to `/state`.
Existing retained discovery definitions are left intact and stop receiving state
updates. Reenabling state republishes discovery through the usual config/client
refresh. State messages remain unretained QoS 0.

## Radio-load memory budget (2026-10-25)

The user's logs show MQTT errno 12, Wi-Fi timer allocation failures and HTTP
send failure. The caught C++ allocation failure logs about 12 KiB free and a
6 KiB largest block after client teardown. This is memory exhaustion, not an
observed reboot/backtrace in that excerpt. Physical long-run stability remains
unverified. Earlier exception recovery did not itself provide enough headroom.

The ST7789 worker now owns a 240x8 RGB565 DMA band (3840 bytes), replacing its
240x135 full frame (64800 bytes): 60960 fewer live DMA allocation bytes. It renders
and transfers each band serially, waiting for the DMA callback before reuse,
including the final seven-row band. Rotation is sampled once per complete view
and maps global coordinates; panel gaps/orientation and acquisition ownership
remain unchanged. The portable full renderer remains a host reference, never
allocated as a full buffer by the display worker. Host checks compare all pixels
for both orientations and guard every band boundary.

MQTT bounds its unacknowledged QoS outbox to 8192 rather than 32768 bytes. A
caught allocation failure tears down the client and waits 15 seconds before any
new client work, keeping acquisition alive and avoiding immediate recreation.
Host tests require the error/disconnected state during cooldown and reconnection
after it. Actual free/largest heap and display transfer timing require module
validation; the calculated saving is not a measured hardware heap watermark.

## Access-policy save/restart (2026-10-25)

The access mutation now schedules the existing restart mechanism after a
successful save response, exactly as the simulation mutation does. The network
worker executes the restart 1500 ms later; no request callback blocks to reboot.
Persistence and access/CSRF/origin validation precede this scheduling. Policy
permissions still change immediately, and reboot clears temporary grants.

Settings hides its controls and displays an English Restarting page only after
save success, disables further submissions and stops status polling/queued API
requests. It explains None/View/Full consequences and provides a manual readings
link except for None. Errors keep Settings available. No timed redirect or
automatic write retries are used. Physical reboot validation remains pending.

## Waiting for physical Full access (2026-10-25)

`web/access.hpp` provides the static English S1 waiting shell. Denied `/` and
`/settings` browser requests receive it as HTML; `/unlock` serves the same shell.
Dashboard/settings requests that encounter 403 navigate to `/unlock`. Existing
API denials remain JSON and do not reveal protected data.

The shell polls non-API `/access-check` sequentially every two seconds, with an
eight-second deadline. The response has an empty body: 204 only when current
access permits Full, otherwise 403 (always 403 in setup). It reveals no policy
name, config, diagnostics or tokens; it cannot grant access. The browser replaces
its location with `/` on 204, and tolerates reconnects while waiting. Both the
page and probe are the user's explicit exception to the None web-UI block.
S1 must be configured and electrically functional; no button bench test is claimed.

## Web Wi-Fi and net-power overview (2026-10-25)

The existing guarded status/values responses add `wifi_rssi_dbm` (null when
unassociated/query fails) and `gauge_limit_w`. RSSI comes from the current STA
association without SSID/credential exposure or extra client polling. The browser
shows dBm and a clamped estimate `2*(RSSI+100)` percent, labelled as a signal
estimate; grade thresholds are -50/-67/-75 dBm. It is not a throughput metric.

`display/dashboard.hpp` owns a four-byte atomic gauge limit, default 5000 W,
valid 100-1000000 W. Boot reads the bounded four-byte `gauge_w` NVS blob, falling
back to default when missing/invalid. Display GET/PUT includes this limit with
rotation; strict types, unique known keys and integer/range validation precede
one NVS commit. Update active values only on commit success, without restart.
View clients receive the limit with their readings, Full/CSRF is required to
change it. Settings takes the symmetric half-range in kW.

The browser derives net watts from fresh, finite, nonnegative, scaled unit-27
import/export values with matching sample times. The gauge marker clamps to the
configured bounds, while its numeric value stays unclamped. User-requested
exception: missing export OBIS uses a labelled assumed 0 W, permitting users
without PV to see the import gauge. Present-but-stale/invalid export and missing
import still make the gauge unavailable. No synthetic zero enters the processor,
raw history or MQTT. Disconnection marks the view unavailable. Rendering uses
HTML/CSS and the existing two-second reading request, with no firmware raster
buffer, external assets or dependencies. Browser visual/device RSSI acceptance
remains pending; host tests cover value cases and configuration persistence.

## M5 serial configuration (2026-10-26)

`config/serial.cpp` owns a fixed versioned NVS record, mutex-protected saved
settings and immutable boot settings. Initialize it before any network workers,
using menuconfig as missing/invalid-NVS fallback. Main passes only boot settings
to UART. GET/PUT `/api/v1/serial` are Full and station-origin protected; mutations
also require CSRF, bounded JSON and strictly validated unique fields. Commit
success precedes saved-state update and response, then the existing delayed
restart applies the settings without reconfiguring UART under acquisition.
A corrupt record remains repairable without erasing other settings. Simulation
selection is unchanged; this is the first M5 delivery, not full M5 completion.

## Real-data topic simplification (2026-10-26)

UART uses the configured base directly; replay keeps `<base>/simulation`.
Every worker publication, state/raw/HDLC/summary/OBIS/availability and LWT uses
this selected root. Discovery is rebuilt with the corresponding root while
normal/simulation entity identities stay stable. Producer/drain source checks
and queue resets still prevent cross-source publication. The saved root is not
rewritten; arbitrary configured prefix components remain untouched.

## Color LCD overview (2026-10-26)

The 240x135 ST7789 display keeps a fixed power card rather than paging OBIS
readings. `make_view` formats net import minus export from matching attribute-2,
finite scaled unit-27 readings, using labelled import/export and W/kW. The prior
user-authorized absent-export zero rule applies to this derived display only.
Missing import, malformed/unit-incompatible or mismatched readings yield waiting;
last valid stale values remain visibly amber. Phase voltages use attribute-2
unit-35 values, each with its own stale marker/color. Raw values/unknown OBIS
remain in the processor and HTTP diagnostics, never guessed as W or V.

The palette uses wire-order RGB565: dark background, red import, green export,
amber stale, white main values. Wi-Fi arcs show connection and RSSI strength;
MQ is green connected, amber connecting/disconnected/waiting, red error, gray
disabled. RX pulses green for 400 ms on an increased decoded-notification count,
including replay. At the existing 250 ms display submission cadence, rapid
notifications can coalesce; invalid frames do not trigger a successful-decode pulse.

The network worker samples RSSI every two seconds outside its state mutex;
acquisition only copies cached status with zero-wait locking. Cached footer/radio
state survives mutex contention. MQTT status uses atomic reads only. S1/setup/IP
remain in the footer and never obscure phase values. The renderer uses bounded
primitives and the existing eight-row DMA buffer, with full-image rotation.
`View` stays <=256 bytes, with no new heap buffer/task. Host previews are not
physical LCD color/refresh validation; user acceptance is still needed.

## M5 read-only diagnostics (2026-10-26)

`web/diagnostics.hpp` consumes existing guarded values/raw snapshots sequentially,
without an additional server task/history/buffer. View/Full can open diagnostics;
None receives the existing S1 shell. Each request rechecks access, and a 403 during
either half clears all rendered data before navigating to the unlock page. Device
strings use textContent. Raw serialization uses checked RAII cJSON and rejects
incomplete allocation with 503. Snapshot limits remain four 256-byte raw chunks
and 64 presentation readings; current counters/latest parser error are not an
error log. The page explicitly describes chunk boundaries and separate snapshots.
The user removed diagnostic export/import/copy/download and advanced replay
pause/speed/loop-toggle from M5. Older architecture plans for those are historical.

## M6 OTA (2026-10-26)

`firmware/update.cpp` validates bounded application prefixes/date versions,
selects a non-running OTA slot and confirms a pending app after startup. The
factory/NVS addresses stay fixed; otadata and two OTA slots replace part of the
previous unmounted storage reserve. Bootloader rollback is enabled. Main calls
the startup tick only from acquisition after network initialization status is
known. A factory image is not an OTA pending image. Unreadable state is retried.

`http/firmware.cpp` owns the synchronous bounded upload: Full/origin/CSRF guard,
1024-byte stream buffer, 288-byte prevalidation prefix, 3 MiB/180-second cap,
zero selection before SDK validation, RAII abort on failure, repeated access
checks and delayed restart after boot selection. It yields between writes;
acquisition/display/MQTT keep their owners. The update page suspends polls during
upload and does not retry writes automatically. Image hashes are integrity
checks, not publisher signatures. No app-password/security model change is made.
`VERSION`/PROJECT_VER populate the actual SDK app descriptor; build-firmware.py
packages only the application binary and verifies its date/project/hash. M6
upgrade/rollback is accepted by user report on 2026-10-26; see M6.md for
evidence limits. Former hardware/release is M7.
