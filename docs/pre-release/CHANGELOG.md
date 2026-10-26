# Changelog

## Personal-data cleanup

- Remove original capture files; use explicitly anonymized test fixtures with
  placeholder identifiers 12345678/1234567 and recomputed frame FCS checksums.
- Remove personal copyright name and MQTT example prefix from current files.
- Native protocol/simulation, web and HTTP/MQTT host suites pass after cleanup.
- Existing Git history, author metadata and local historical backups are not
  rewritten by this source cleanup. Device NVS and broker retained data are
  outside this repository operation.

## M6 accepted (2026-10-26)

- User confirms the proposed USB migration/web update/version/settings/MQTT,
  invalid-image and rollback checklist works. Close M6 with local v0.6.0.
- Acceptance is based on user report; exact flashed hash and detailed fault-test
  procedures were not supplied. No assistant flashing or public publication.
- M7 remains hardware/electrical, prolonged real-meter, clean-install and release
  validation. Earlier pending-acceptance notes below describe implementation time.

## M6 web firmware update; hardware/release moves to M7 (2026-10-26)

- Add Full/origin/CSRF-protected application .bin upload in Settings, streamed
  through a bounded buffer to an inactive OTA slot. Recheck access during upload,
  validate ESP32/project/date/hash metadata and SDK image integrity, and select
  the new boot slot only on success. Abort partial writes; reject repeated upload
  while restart is pending. Display the English restart page after success.
- Add rollback-enabled factory + two OTA slots on the 16 MiB layout, keeping
  NVS/PHY/factory addresses. One USB installation is required to migrate the old
  factory-only layout. Confirm a new OTA app after startup checks; fail back on
  failed startup. No physical update/rollback or agent flash is claimed.
- Date firmware versions come from VERSION (2026.10.26), with a build override.
  Add typed scripts/build-firmware.py producing an application-only .bin plus
  SHA256/JSON sidecars; verify the image and avoid cached-version mismatches.
- Checks: binary fragmentation/fidelity, access/expiry/CSRF/origin, size/header/
  date/chip/project, SDK validation/write/select failure, timeout/restart and
  startup rollback-control tests; UI upload/reject/restart, packager corruption/
  date/hash checks, HTTP/MQTT ASan/UBSan and actual ESP-IDF builds for 2026.10.26
  and 2026.10.20 pass. Real-device M6 acceptance remains pending; no M6 tag.

## M5 accepted (2026-10-26)

- User reports completing the requested device tests with satisfactory results.
  Close revised M5 and record local v0.5.0; removed export/import/replay-control
  features remain out of scope. Source df59135 passed native/web/HTTP-MQTT and
  ESP-IDF checks. Exact flashed hash, durations and electrical traces not supplied.
- M6 remains for hardware/electrical, prolonged real-meter/installation and
  licence/release validation. No agent flash or public publication.

## Readable web measurement labels (2026-10-26)

- Change known measurement names to Import/Export, Voltage L1-L3 and Current
  L1-L3, including per-phase power/energy labels. Use Simulation in dashboard
  and diagnostics status. Shared presentation also updates HA friendly names;
  OBIS identifiers, values, units and MQTT topic paths are unchanged.
- Embedded web/native/HTTP-MQTT checks and ESP-IDF build verify the change using
  isolated tracked fixtures; no hardware flash.

## Sentence-case LCD labels (2026-10-26)

- Display Import, Export, Balanced, Waiting, Simulation, Stale and WiFi/S2
  messages in sentence case. Keep acronyms/units such as MQ, RX, UART, L1 and W.
- Add original lowercase 5x7 glyphs instead of forcing every letter to uppercase;
  this also renders kW correctly. Preserve colors, rotation and DMA bounds.
- Native tests and ESP-IDF build pass; host RGB preview inspected. No flash.

## Revised M5: read-only diagnostics page (2026-10-26)

- User removes previously unimplemented M5 items 1/2/3/4/5/7: export/import,
  replay pause/speed/loop-toggle and capture copy/download. Keep only item 6
  alongside the already implemented settings/simulation features.
- Add View/Full Diagnostics linked from readings/Settings: bounded raw tail,
  decoded scalar/string values with OBIS/type/scaler/unit/age and parser counters/
  latest error. Use sequential existing API requests and no new firmware buffer.
- Clear stale snapshots on unavailable reads; clear and open S2 waiting on access
  loss. Insert meter data as text, never HTML. Harden raw cJSON allocation cleanup
  so incomplete serialization returns 503. Describe history/snapshot limits.
- Checks: UI sequential polling, raw fidelity, hostile strings, errors/denial,
  server access/setup/memory failure, HTTP/MQTT ASan/UBSan and ESP-IDF build.
  Device visual/live acceptance remains pending; no M5 tag or hardware flash.

## Color LCD power overview and activity/status icons (2026-10-26)

- Replace reading pagination with a stable net-import/export power card: large
  W/kW number, red import/green export, neutral balanced/waiting and amber stale.
  Put small L1/L2/L3 voltages underneath; keep network/IP/S2 in the footer.
- Add Wi-Fi arcs with sampled signal strength, colored MQ connection state and
  a 400 ms RX pulse per new decoded notification (including simulation).
  Broker/radio calls stay out of acquisition; footer cache avoids false drops
  on a contended network mutex. Unknown/scaler-invalid values are not invented.
- Keep the 3840-byte DMA band, global rotation and a display mailbox <=256 bytes.
  Native power/scaler/voltage/stale tests, RGB band/rotation/bounds checks,
  reception pulse/icon checks, protocol and HTTP/MQTT ASan/UBSan, embedded web
  tests and pinned ESP-IDF build pass (tracked fixtures in an isolated copy).
  Host RGB previews inspected; physical color/legibility/refresh acceptance and
  S2/icon operation remain to be confirmed. No hardware flash.

## Remove real-data normal topic suffix (2026-10-26)

- Publish real UART data directly below the configured base, e.g.
  `openp1/example/active`. Keep replay under `/simulation/active`.
- Apply the same roots to state, hex, OBIS, availability/LWT and discovery,
  preserving identities/settings and source guards. Update Settings examples.
- Service tests cover custom/default prefixes, source switches and discovery.
  Use tracked capture fixtures in isolation without changing local deletions.
  Embedded web/HTTP-MQTT ASan/UBSan checks and ESP-IDF build pass. No flash.

## Reference UART defaults (2026-10-26)

- Match the reference ESP8266 defaults: 115200 baud, 8 data bits, no parity,
  one stop bit and inverted RX. Update the portable/NVS fallback, menuconfig,
  sdkconfig defaults, local generated baud and Settings initial field.
- Preserve existing saved runtime UART settings. MQTT settings and source
  selection are unchanged. Tests use tracked capture fixtures in an isolated
  copy. Native/web/HTTP-MQTT sanitizer checks and pinned ESP-IDF build pass;
  generated headers confirm the requested UART profile. No hardware flash.

## M5 first delivery: persisted meter UART settings (2026-10-26)

- Add fixed versioned NVS settings for baud/parity/stop bits/inversion, using the
  existing board profile as fallback and immutable UART boot configuration.
- Expose Full/origin/CSRF-guarded, bounded serial GET/PUT and Settings form. Save
  commits then displays a restart page and schedules restart; source is unchanged.
- Checks: persistence/reboot/corrupt-record/commit-failure, API validation/access,
  UI/restart, HTTP/MQTT ASan/UBSan and ESP-IDF build pass. No new tasks/large buffers,
  flash or M5 tag. Export/import and advanced replay remain unimplemented.

## M4 accepted; M5 opened (2026-10-26)

- User accepted the remaining M4 checks and authorized M5. Record local v0.4.0
  closure; refreshed native/web/HTTP-MQTT sanitizer checks and ESP-IDF build pass.
- Record M5 scope without claiming implementation/completion. No independent
  hardware measurements, agent flashing or public publishing.

## Net-power gauge presentation (2026-10-26)

- Place net grid power before Wi-Fi signal. Use green on the export/left side
  and red on the import/right side. Default new/missing gauge settings to ±5 kW;
  retain explicitly saved ranges. Update initial labels and Settings defaults.
- Embedded web/HTTP tests and pinned ESP-IDF build verify the change; no flash.

## Wi-Fi quality and configurable net-power gauge (2026-10-25)

- Add the Wi-Fi RSSI/estimated-quality panel above connection/source. Report
  unavailable RSSI explicitly; the percentage is an estimate, not throughput.
- Add a centered net-power gauge (import 1-0:1.7.0 minus export 1-0:2.7.0),
  default -10 to +10 kW, with Export/Import watts below. Clamp the marker only;
  retain the actual numeric value outside the range. Require fresh matching
  scaled W readings. User-requested exception: absent export means 0 W for the
  gauge, visibly labelled; stale/invalid transmitted export does not become zero.
- Full/CSRF-protected Display settings persist the symmetric gauge limit
  (0.1-1000 kW) without reboot. View can read it with status/values; no extra
  polling connection or large buffer is added. Invalid saved limits use 10 kW.
- Checks cover signal unavailable, gauge direction/bounds/missing/stale/scaler
  cases, settings validation/persistence/commit failure and guarded HTTP access.
  Embedded web, HTTP/MQTT ASan/UBSan tests and pinned ESP-IDF build pass.
  Hardware RSSI/visual acceptance remains pending; no flash.

## Physical unlock waiting page (2026-10-25)

- Replace denied browser navigation to main/settings with an English S2 waiting
  page. Poll an empty status-only 204/403 probe, then automatically open `/`
  when Full access is confirmed; reconnect gracefully while waiting.
- Keep protected APIs guarded and deny the probe in setup mode. The public
  static shell/probe reveal no config, measurements, diagnostics or tokens.
- Host checks cover None/View/Full, grant/expiry, setup refusal and automatic
  navigation/reconnect; embedded web, HTTP/MQTT tests and pinned ESP-IDF build
  pass. Physical S2 acceptance remains pending; no flash.

## Restart after access-policy save (2026-10-25)

- After a successful policy save, Settings shows an English "Restarting…" page,
  stops polling/submissions and explains the saved policy's effect. Saving None
  hides the readings link; View explains that Settings needs Full access.
- Schedule the existing network-worker restart 1500 ms after a successful save
  response, as for simulation settings. Failed saves do not schedule a restart.
  Restart ends temporary Full access; policy enforcement remains immediate.
- Checks cover failed/successful saves, restart timing, the English page state
  and stopped polling; web/HTTP/MQTT tests and pinned ESP-IDF build pass.
  Hardware restart acceptance remains pending; no flash.

## Recover RAM headroom under radio load (2026-10-25)

- User logs show MQTT ENOMEM, Wi-Fi timer allocation failures and HTTP send
  errors while replay continues; previous allocation guards did not provide
  enough headroom.
- Replace the 64800-byte display DMA frame with a 3840-byte eight-row band,
  releasing 60960 allocation bytes. Wait for DMA before every reuse and preserve
  full-image rotation, panel geometry and latest-view task ownership.
- Limit pending MQTT QoS traffic to 8 KiB and wait 15 seconds after a caught
  allocation failure before recreating the client.
- Checks: host reference pixel equality in both orientations including band
  guards/final band, native suite, HTTP/MQTT cooldown/recovery with ASan/UBSan and pinned
  ESP-IDF build. Physical heap/refresh/soak validation remains pending; no flash.

## Explain MQTT publication options (2026-10-25)

- Group publication switches together, placing Home Assistant Discovery directly
  before JSON state. Add concise descriptions and illustrative topic/payload
  examples for state, summaries, hex, OBIS and the ID label, with accessible
  help associations. Explain the discovery/state dependency and direct-APDU case.
- Publication behavior and saved settings are unchanged. Embedded web tests
  and pinned ESP-IDF build verify the embedded page; no hardware flash.

## Optional MQTT JSON state (2026-10-25)

- Add independent persisted "Publish JSON state topic (/state)" control,
  default enabled. Skip state JSON allocation/publication while disabled;
  preserve summary/hex/OBIS/availability and HTTP readings.
- Suppress new Home Assistant discovery while state is disabled; retained
  existing definitions remain, with the dependency explained in Settings.
- Config v5 uses the remaining padding byte without growing NVS storage.
  Migrate v1-v4 with state enabled and all other settings preserved.
  Checks: UI/API flag validation, service absence of state/discovery while hex
  remains active, migration tests and pinned ESP-IDF build pass. No flash.

## Direct compatibility topics and optional OBIS values (2026-10-25)

- Remove `/text/` from compatibility measurement/hex topic paths. `/raw` carries
  hexadecimal frames; the old JSON chunk publisher remains removed.
- Add independent, default-off OBIS publication: `<prefix>/<mode>/1-0:51.7.0`
  with scaled finite numbers/plain strings and no unit suffix. Omit stale,
  malformed or oversized values. Keep normal/simulation isolation and state JSON.
- Config v4 keeps the same NVS size; migrate v1/v2/v3 preserving broker settings,
  summary/hex flags and credentials, with OBIS disabled. No hardware flash.
  Checks: embedded UI, formatter bounds/scalers/stale cases, HTTP/MQTT service
  and migration tests, ASan/UBSan and pinned ESP-IDF build pass.

## Separate text measurements and hexadecimal output (2026-10-25)

- Split MQTT text measurement topics from raw/HDLC hexadecimal topics into
  independent persisted switches. Disabled hex output does not allocate capture
  resources; state JSON and the removed JSON raw publisher remain unchanged.
- Config v3 reuses a reserved byte without changing storage size. Migrate v2
  by copying the former shared enable to both switches, preserving behavior and
  connection settings; v1 retains both formerly enabled text groups.
- API/UI tests cover independent flags; service tests cover measurements-only
  and hex-only publication, source isolation and v1/v2 migration.
  Embedded web/HTTP/MQTT tests and pinned ESP-IDF build pass. No hardware flash.

## Remove MQTT raw JSON publication (2026-10-25)

- Remove the Settings toggle, configuration API field, MQTT raw-chunk producer,
  queue, serializer and drop counter. No MQTT `/raw` JSON publication remains.
- Preserve `/text/raw`, `/text/hdlc`, state JSON and HTTP raw diagnostics. Keep
  an unused byte solely for saved NVS layout compatibility; old enabled raw
  settings cannot reenable publication, and new `raw` API fields are rejected.
- Service checks reject `/raw` publications on either source branch while
  retaining exact text frames/APDUs. Broker/credential configuration is preserved.
  Embedded web, HTTP/MQTT integration tests and pinned ESP-IDF build pass.
  No hardware flash or milestone tag.

## Replace MQTT debug console with status (2026-10-25)

- Remove console UI, capture/history implementation, instrumentation and debug
  GET/PUT routes. Replace them with a small Full/station-origin guarded MQTT
  status API and five-second Settings refresh: broker connection, boot-lifetime
  accepted publications, publish failures/error counts and last numeric error.
  No topic/payload history or credentials are retained by these counters.
- Show successfully decoded meter notifications since boot on the dashboard,
  including simulation. Failed frames/chunks/segments are not counted as messages.
- Counters cover all locally published topics and reset on restart; client
  acceptance is not subscriber delivery. Reconnect preserves the last error.
  Checks: native suite with the tracked capture fixture (local user capture edit
  preserved), embedded web tests, HTTP/MQTT service tests including rejected
  publication/error codes/access guards, ASan/UBSan and pinned ESP-IDF build pass.
  Hardware/broker visual acceptance remains pending; no flash or milestone tag.

## Lightweight uptime status (2026-10-25)

- Status/uptime no longer allocates the complete reading/encoded-value snapshot;
  copy only counters, flags and IP under the existing lock. Values retain their
  isolated snapshot and report `snapshot_memory` if that allocation fails.
- Include HTTP status in settings API errors instead of hiding it behind the
  generic server message. Physical cause of the user's intermittent failures
  remains unconfirmed; this removes a known large allocation from uptime reads.
- Regression checks refuse snapshot allocation: status still succeeds, values
  return a specific 503, and None/View/Full policies remain enforced.

## Uptime and web request recovery (2026-10-25)

- Show read-only device uptime and free RAM in Settings using the existing
  access-controlled status API; refresh every 10 seconds and mark failed reads.
- Serialize settings requests instead of opening five startup requests against
  three server sockets. Add eight-second request/body deadlines, continued
  polling after transient errors and explicit unknown save-result messages.
  Never retry writes automatically.
- Embedded script checks cover formatted uptime, unavailable/recovered status,
  concurrent request serialization and timeout recovery; pinned ESP-IDF build
  passes. Physical diagnosis of
  intermittent availability remains pending; no hardware flash.

## Independent MQTT raw formats (2026-10-25)

- The raw option now controls JSON `/raw` only. Text output independently
  enables `/text/raw` and `/text/hdlc`, allowing text frames without JSON chunks.
  Clarify both controls in Settings; preserve saved broker and topic settings.
- Service regression checks require text frames with JSON raw disabled and
  reject JSON raw publications in that interval. No device configuration or
  hardware flash performed.

## Dashboard and settings feedback (2026-10-25)

- Unify the dashboard/settings light palette, card layout, readable measurement
  table, mobile controls and settings section navigation. Add pressed, hover
  and keyboard-focus states without external assets.
- Show saving/success/error feedback beside each save button and in the page
  status; disable action buttons during requests and ignore duplicate submits.
  Report default-sequence loading separately from saving it.
- Checks: embedded script tests including slow/error/duplicate save cases,
  HTTP/SSE/MQTT host tests and pinned ESP-IDF build. Browser visual acceptance remains pending because
  the local preview socket was denied. No hardware flash.

## Exception-profile build cache (2026-10-25)

- Diagnose `undefined reference to __eh_frame` as a stale preprocessed linker
  script after enabling C++ exceptions. Document clean/rebuild without deleting
  saved configuration or adding a dummy unwind symbol.
- Check: clean and full pinned ESP-IDF build pass in the normal `.pio/build`
  directory; regenerated linker script defines `__eh_frame`. No hardware flash.

## Recover decoder/MQTT allocation failures (2026-10-25)

- Resolve matching user backtraces to A-XDR node growth and MQTT JSON key
  allocation under the no-exceptions profile.
- Allocate debug history lazily; exact-reserve parsed nodes/children; use fixed
  JSON keys/discovery metadata. Static RAM drops from 88736 to 68632 bytes.
- Enable exceptions with a warmed 2048-byte emergency pool, preserve good
  readings/raw bytes on allocation failure, retry MQTT and return unavailable
  or close affected HTTP streams. Free incomplete JSON without publication.
- Cap Wi-Fi dynamic RX/TX buffers at 10/8 and expose runtime heap statistics.
- Checks: allocation refusal/recovery and JSON cleanup tests, native/web and
  HTTP/MQTT ASan/UBSan tests, generated profile and pinned ESP-IDF build.
  Original-fixture native verification preserves the local user capture edit.
  Physical stability/throughput remain unverified; no flash or milestone tag.

## Additional MQTT text format (2026-10-25)

- Add optional text compatibility topics under the configured prefix and active
  normal/simulation branch, using `text/` alongside the existing JSON topics.
  Include exact observed frame hex, reassembled LLC/APDU hex, configurable ID
  label and active/voltage/current/metering summaries in the supplied order.
- Keep one broker/TLS/credential configuration, migrate the old NVS MQTT layout
  to v2, and expose text enable, label and drop count in settings. The raw option
  gates hex publication. Preserve source isolation and non-retained QoS 0 text.
- Use shared-parser observers and bounded nonblocking queues/mailbox; never
  construct HDLC frames from direct APDUs or zero-fill missing readings. Count
  diagnostic losses and omit missing/stale/incompatible summary groups.
- Checks: supplied numeric outputs, frame/reassembly byte fidelity with byte-wise
  fragmentation/embedded flags/bad CRC, both sources/custom prefixes, migration,
  coexistence/disabling/oversize, web and ASan/UBSan regressions, pinned ESP-IDF
  build. Live broker/consumer acceptance remains pending. No flash or tag.

## Runtime simulation settings (2026-10-25)

- Move simulation from Kconfig to Full-access Settings with a persisted enable
  flag, explicit HDLC/LLC-APDU format and editable bounded sequence. Default off
  on initial setup. Save and restart applies the selected source before network
  and MQTT startup; the restart is requested only after the save response.
- Supply the meter APDU with only serial/GR identifiers changed to 12345678 and
  1234567. Provide a restore-default button. Preserve the shared pipeline, raw
  bytes, 64-byte/20 ms chunks and 3.5-second repeat pause. Both input paths are
  always compiled; simulation build options have been removed.
- Validate at most 4096 text characters / 2048 raw bytes without parser heap
  allocations; reject malformed settings, enforce Full/origin/CSRF guards and
  preserve saved data on failed writes. Keep distinct MQTT/HA channels.
- Checks: native simulation fidelity/timing/malformed cases, HTTP and service
  defaults/persistence/failure/access/bounds, embedded UI actions, ASan/UBSan
  (leak detection disabled under sandbox tracing) and pinned ESP-IDF build.
  Physical save/restart/UART-return acceptance remains pending. No flash or tag.

## Separate normal/simulation MQTT channels (2026-10-25)

- Enable MQTT during replay as explicitly requested. Publish state, optional raw
  data and availability under `<prefix>/normal/...` or `<prefix>/simulation/...`.
  Include simulation/source metadata; isolate Home Assistant client/device/entity
  identities and visibly name simulation entities. Normal discovery identities
  remain stable while their topic references move to the normal branch.
- Recreate the client and LWT on source changes; clear queued raw data and check
  source at both enqueue and send. Initialize the network snapshot source before
  worker tasks start to avoid any startup normal-channel publication in simulation.
- Checks: native/web/HTTP/MQTT tests, including startup in either mode, custom
  prefixes, switching both ways, queue isolation, LWT, discovery and TLS gating;
  pinned ESP-IDF builds for simulation and UART. Physical broker/HA acceptance
  remains pending; no flash or milestone tag. Direct MQTT subscribers must move
  from `<prefix>/state` to `<prefix>/normal/state` (and raw/availability likewise).

## Fix debug console allocation abort (2026-10-25)

- Decode the user backtrace against the matching ELF: a failed allocation of
  the full debug history called abort despite `new(std::nothrow)`.
- Send debug entries individually using a fixed record window, removing the
  full history copy and full-response JSON allocation. Report records lost
  during a read and recheck Full access before every response chunk.
- Use checked malloc and placement construction for fallible HTTP/MQTT
  allocations; allocation failures return unavailable or close a response.
- Checks: native/HTTP/MQTT/web regressions, 100 full-history polls with large
  allocations rejected, injected allocation failures, JSON escaping, record
  overwrite/clear and mid-response access expiry; pinned ESP-IDF build.
  Physical console soak test remains pending. No flash or milestone tag.

## Repeating simulation (2026-10-25)

- Repeat the configured simulation sequence indefinitely, pausing 3.5 seconds
  after each cycle. Preserve the shared pipeline, raw bytes, history, last valid
  readings and the MQTT simulation publication block. Keep timeout, display and
  network snapshot updates running during the pause.
- Checks: native replay tests verify identical bytes/layers on the next cycle
  with refreshed timestamps, including fragmented and malformed input; pinned
  PlatformIO ESP-IDF simulation build. Physical timing acceptance remains
  pending; no hardware was flashed or milestone tagged.

## MQTT debug console (2026-10-25)

- Add temporary debug capture, clear history and pause updates to Settings.
  Show uptime timestamps, connection/login attempts and broker acceptance,
  numeric connection errors, outgoing topics/payload previews, QoS, retain,
  client results and QoS acknowledgement message IDs. Also trace Wi-Fi, S2
  presses/releases/actions and saved access/display/MQTT settings.
- Bound RAM history to 24 entries, with explicit truncation and loss counters.
  Omit credentials and certificates; protect all console requests with Full
  access, origin checks and CSRF on mutations. Render payloads as text.
- Checks: native ring bounds/overwrite/contention tests, HTTP access/input tests,
  MQTT service tests checking real outgoing traces and credential omission,
  embedded console tests and pinned PlatformIO ESP-IDF build. Physical S2 and
  live broker/TLS acceptance remain pending; no flashing or milestone tag.

## Persistent screen rotation (2026-10-25)

- Add Display / Rotate screen 180° to Full-access settings, with bounded,
  CSRF-protected GET/PUT `/api/v1/display` configuration.
- Save the boolean in NVS before applying it; missing settings keep the original
  orientation. Reverse RGB565 pixels in the display task before DMA, preserving
  the existing panel offsets and acquisition ownership. No reboot is required.
- Checks: pinned PlatformIO ESP-IDF firmware build, native, HTTP/SSE/MQTT
  and embedded web tests pass, including strict
  boolean validation, None/View/provisioning denial, NVS commit failure and
  browser loading/submission. Physical rotation/reboot acceptance remains pending;
  no hardware was flashed and no milestone tag was created.

## Conditional MQTT/TLS settings (2026-10-24)

- Default TLS to off and port to 1883 as requested; preserve existing saved
  TLS/port choices, and keep verification mandatory when TLS is enabled.
- Show connection settings only with MQTT enabled, and certificate/time fields
  only with TLS enabled. Hidden groups are disabled so irrelevant browser
  validation cannot block disabling MQTT.
- Preserve hidden values and omit inactive fields from updates. Disabling MQTT
  writes only its enable flag, retaining the saved connection settings.
- Embedded-script tests cover loaded state, toggling both controls, hidden-field
  preservation, invalid inactive port values and partial submissions.

## English application text (2026-10-24)

- Translate the dashboard, provisioning wizard, settings, access feedback,
  errors and accessibility labels to English; all pages declare lang=en.
- Use English serial/GR, unknown-object and import/export tariff labels in
  API/dashboard/Home Assistant presentation. Update documentation and tests.
- Keep original OBIS, exact capture bytes and string/UTF-8 handling intact.
  Embedded-script, native and HTTP tests and the pinned firmware build checked.
  No agent flashing performed.

## Full access by default (2026-10-24)

- Set initial access policy to Full at the user's request. Existing unversioned
  View defaults migrate once to Full; saved None is preserved. Persist a defaults
  marker so future explicit View/None choices survive reboot without migration.
- Policy changes write the marker and policy in the same NVS commit. Invalid policy or
  defaults marker values fail closed. No change to setup-only isolation.
- Native migration cases and existing access/HTTP tests cover the policy rules;
  firmware build checked. No agent flashing performed.

## M4 settings-access feedback and short-press polling (2026-10-24)

- Reduce the network/S2 worker delay from 100 ms to 10 ms: the old cadence
  combined with 50 ms debounce could miss an ordinary brief press/release.
- Show effective access and remaining grant time directly in the dashboard.
  Distinguish Full-required from canonical station-origin rejection when
  opening settings, without weakening policy or origin checks.
- Add a 100 ms-tap polling regression, dashboard permission/countdown checks
  and settings-access reason tests. Physical button recognition still requires
  confirmation on the board; no agent upload performed.

## Unreleased — M4 software (2026-10-24; live acceptance pending)

- Implement persisted None/View/Full guards, monotonic 300-second S2 grant,
  expiry during mutation receipt and per-chunk revocation on bounded SSE streams.
- Add debounced GPIO35 S2 with eight-second visible Wi-Fi-only reset countdown,
  boot-held suppression and no short action after long release. Disabled by
  default until the external/onboard pull-up is verified.
- Add Full-only policy/MQTT settings with CSRF/Host/Origin checks, bounded shallow
  JSON, per-second write limiting and secret-safe reads. Add View raw tail API.
- Implement independent MQTT worker, NVS settings, exact optional unretained raw,
  bounded outbox/queues, retained LWT/availability and HA discovery. MQTT does not
  read access policy and blocks replay/provisioning. TLS verifies CA and hostname,
  waits for valid time and has no insecure fallback; plain MQTT is explicit.
- Checks: native grant/button/presentation/protocol suites, actual HTTP/SSE and
  MQTT service with host clock/transport/NVS doubles, JSON secret/mutation tests,
  embedded dashboard/wizard/settings scripts. HTTP/SSE/MQTT host tests also pass
  AddressSanitizer/UndefinedBehaviorSanitizer (leak detection disabled). PlatformIO builds cover replay
  and UART with S2 enabled; original local replay settings restored afterward.
- No physical S2, live broker/TLS rejection or HA acceptance is claimed. Follow
  docs/M4.md before M4 tag/M5. No agent flashing, remote publishing or new library.

## M3 acceptance (2026-10-24)

- User explicitly confirms M3 validated. Previous native, JavaScript, HTTP and
  pinned firmware checks pass. Exact phone versions and bench logs were not
  supplied; no agent hardware execution is claimed. Proceed to M4.

## M3 OBIS columns, tariff labels and identifiers (2026-10-24)

- Separate measurement/OBIS columns; show OBIS as `A-B:C.D.E`, hiding only
  F=255 and preserving original channel values and other F suffixes. API keeps
  canonical `obis` and adds `obis_display`.
- Generate `Import +A tariff N` / `Export -A tariff N` labels for transmitted
  tariff indices, with total labels for E=0; no four-entry tariff limit.
- Publish decoded string/octet values alongside numbers, including the two
  captured identifiers labelled serial (96.1.0) and GR (96.1.1), pending user
  confirmation of that meter-specific correspondence. Preserve leading zeroes;
  invalid UTF-8/control bytes render as explicit lossless hex instead of zeros.
- Bound presentation storage to 64 readings and 8192 exact encoded value bytes.
  HTTP uses a heap copy, keeping the larger snapshot off its task stack.
- Checks: real capture through shared snapshot/API (16 objects including both
  identifiers), tariff/OBIS/string boundary native tests, dashboard/wizard JS
  regressions, native protocol/input suites and ESP-IDF firmware build.
  Physical UI acceptance remains pending; no agent flashing or release tag.

## M3 original provisioning wizard (2026-10-24)

- Replace the basic setup form with an original three-step offline wizard:
  network cards showing signal/security, manual SSID entry, Wi-Fi credentials,
  and live connection progress with failure/retry and station-IP handoff.
- Implement asynchronous Wi-Fi scans on the network worker: bounded 16-record
  cache, ten-second refresh/deadline, polling responses and scan/connect guards.
  HTTP no longer waits for the radio scan. Network worker stack is 6 KiB.
- Add setup-only status with fixed states and no credentials/diagnostics.
  After NVS persistence, retain setup-only AP for ten seconds to show success
  and IP, then close it; meter endpoints stay blocked throughout setup.
- Redirect noncanonical captive-detection hosts to the manual setup address.
  Keep body/header limits, CSRF/Origin checks and nonblocking acquisition.
- Implemented independently in ESP-IDF/C++17, without importing NetWizard
  code, UI assets or dependencies and without adding Arduino.
- Checks: embedded script wizard/dashboard regressions, native suites, real
  HTTP handlers with host transport doubles and bundled cJSON, firmware build.
  Physical mobile/radio acceptance remains pending; no agent flash or M3 tag.

## M3 browser header limit fix (2026-10-23)

- Diagnose the exact ESP-IDF HTTP 431 message “Header fields are too long”:
  the generated configuration still used the default 512-byte header limit.
- Set project defaults and the existing local generated configuration to 4096
  bytes, preserving replay/serial settings. Document menuconfig migration for
  other existing checkouts; defaults alone do not update generated configs.
- Verify the effective generated configuration and firmware compilation. Header
  size stays bounded; body, credential and access limits remain unchanged.
  Physical browser verification remains pending. No agent upload performed.

## M3 scan interface fix (2026-10-23)

- Replace datalist-only Wi-Fi suggestions with a visible native select, suitable
  for captive portal windows; selecting a network fills the manual SSID field.
- Show scanning, empty results, retry delay and HTTP failure messages; disable
  repeated clicks while scanning and deduplicate names. Log only ESP scan error
  names on failure, without network names or credentials.
- Embedded-script regressions cover selection, deduplication, empty results,
  busy/rate-limit/server failure messages and button state. Firmware build
  checked; physical phone/radio verification remains pending.

## Unreleased — M3 software (2026-10-23; network acceptance pending)

- Wi-Fi-only provisioning AP with captive IPv4 DNS advertised through DHCP,
  HTTP probe redirects, bounded scan and manual SSID entry. Try credentials
  for 30 seconds; persist only after obtaining an IP, then close the AP.
- Persisted station reconnects with capped exponential backoff; outages never
  erase credentials or open provisioning. NVS errors fail closed without erase.
- Embedded responsive dashboard and read-only `/api/v1/status` and `/values`:
  simulation, units/OBIS, monotonic age/timestamps and per-value stale state.
  The screen alternates network information with the last reading pair.
- Acquisition publishes bounded nonblocking numeric snapshots. Every HTTP
  request checks setup/access state; setup exposes Wi-Fi only, None denies
  dashboard/API. Persisted default is View. Bounded JSON, scan throttling,
  same-origin/token provisioning writes; no credential reads or application logs.
- Checks passed: existing native pipeline/protocol/display tests, network
  credential/access/backoff/stale bounds and DNS malformed corpus (10,000
  packets), network AddressSanitizer/UndefinedBehaviorSanitizer (leak detection
  disabled), embedded JavaScript behaviour tests. Pinned ESP-IDF 5.3.1 builds
  checked with display in UART and LLC/APDU replay configurations.
- Build environment: writable temporary PlatformIO core, local pinned platform,
  copied ESP-IDF package and existing Python environment; no global changes or
  downloaded dependencies. User replay settings restored after UART build.
- Android/iOS captive detection, real provisioning, NVS persistence, router
  interruption/recovery and physical endpoint-policy checks remain pending in
  docs/M3.md. No agent flashing or M3 tag. MQTT, S2, full configuration, raw
  diagnostics/export, uploads and streaming remain later milestones.

## Physical display observation (2026-10-23)

- User reports visible black-and-white `OPENP1 UART`, `NO READINGS ERR 0`
  and `NONE` on the physical screen, matching the implemented startup view.
  User also confirms correct orientation and fully visible text without clipping.
  The user subsequently reports the requested replay test successful
  (measurements, pagination, SIMULATION and stale indication).
  M2 display acceptance is therefore complete by user report. No agent flashing performed.

## Development environment note

- Diagnosed upload-port autodetection aborting under Python 3.14 when it loads
  an unrelated installed pioarduino platform. Document explicit port selection
  for CLI/VS Code; verified discovery bypass without opening a serial device.
  No global toolchain changes or hardware upload performed.

## Unreleased — M2 software (2026-10-21; physical acceptance pending)

- Shared portable UART/replay processing after unchanged raw capture: declared-
  length HDLC parsing, HCS/FCS, bounded reassembly of UI and sequenced I frames,
  inactivity timeout and explicit truncation/discontinuity errors.
- Bounded data-notification A-XDR tree, self-describing OBIS tuples and explicit
  push descriptor/value associations, original types/scalers/units/unknown
  objects, atomic last-good per-object samples with exact encoded values and age.
- Four complete real three-frame notifications supplied by the user now pass
  regression tests, including exact first-payload comparison with the separately
  supplied LLC/APDU. Preserve the real fixture bytes/checksums and identifiers.
  First capture decodes to 950 W import, 0 W export, 9281471/5876412 Wh,
  241.7/239.9/242.6 V and 0.30/3.00/1.31 A. No derived phase average is invented.
- Proposed ESP-IDF ST7789 display profile with a separate worker and latest-view
  mailbox, original raster font, paging, simulation label and per-reading stale
  markers. Initialization failures leave acquisition running. Headless option.
  Main acquisition task stack is explicitly 8192 bytes for bounded recursive
  decoding; SPI/DMA waits are isolated to the display task.
- Checks passed: C++17 native hex/input/protocol/display-model suite, all single
  splits of all four real captures, bad HCS/FCS, truncation, missing/reordered
  segments, I sequence wrap/errors, unknown objects/types/units, resource bounds,
  retention of good data and 1000 deterministic malformed-input trials.
  Protocol suite passed AddressSanitizer/UndefinedBehaviorSanitizer (leak
  detection disabled because tracing prevents LeakSanitizer here). Host raster
  preview visually inspected. PlatformIO espressif32@6.9.0 / ESP-IDF 5.3.1 builds:
  UART/display, real HDLC replay/display, LLC/APDU replay/headless. Generated
  replay configuration also checked through the host pipeline.
- At implementation time the physical test was pending. User-reported display
  acceptance on 2026-10-23 is recorded above; meter electrical checks remain pending.
  Wi-Fi, web/API, MQTT, encrypted APDUs, JSONL import and persistent capture
  storage remain unimplemented. See docs/M2.md and test/fixtures/README.md.

## v0.1.0 — M1 (2026-10-21)

- UART1 RX on GPIO27 with configurable baud, 8-bit data, None/Even/Odd
  parity, one/two stop bits and RX inversion through ESP-IDF menuconfig.
- Shared UART/replay raw pipeline: 32 x 256-byte RAM chunks, monotonic
  timestamps, explicit source/layer, overwrite and UART error counters.
- Bounded portable hex/MQTT replay with line-numbered errors, byte-exact
  chunking, explicit HDLC versus LLC/APDU layer and source exclusion.
- Firmware replay simulation selected at build time; UART is not initialized
  in replay mode. JSONL, uploads and runtime controls remain M5 work.
- Checks: native hex/input tests and AddressSanitizer/UndefinedBehaviorSanitizer
  passed (LeakSanitizer disabled: unsupported under this environment's tracing).
  PlatformIO espressif32@6.9.0 / ESP-IDF 5.3.1 builds passed for default UART
  and one-byte LLC/APDU replay, plus non-inverted 8E2 UART. See docs/M1.md for limits and configuration.
- Environment: `.git` is an empty read-only mount. Restored bundled M0 history
  into `.git-local`, no remote; use `git --git-dir=.git-local` locally.
  Build used a writable temporary PlatformIO core and existing local packages
  because the installed core is read-only and external dependency downloads
  are unavailable. No project dependency or platform version was changed.
- No flashing or bench tests. Meter electrical compatibility, baud/polarity,
  board flash capacity and physical UART behaviour remain unverified.
  No protocol decoder, readings, display, Wi-Fi, web UI or MQTT yet.

## v0.0.1-scaffold

- Initial PlatformIO/ESP-IDF structure, configurable board profile and settings model.
- Strict portable hexadecimal parser with host tests.
- Requirements, architecture, proposed API/capture contracts, hardware and Codex handoff.
- No operational meter firmware yet. ESP compilation and all physical checks remain pending.

## MQTT leading slash compatibility

- Accept and preserve a leading slash in the configured MQTT topic prefix, including saved settings and simulation topics. Trailing slashes and wildcard characters remain rejected.
- Checked configuration JSON acceptance/round-trip and invalid-prefix rejection with host tests. No hardware flashing performed.
