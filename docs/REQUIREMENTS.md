# Requirements

## Confirmed choices

- All application text and project documentation must be in English. Preserve incoming meter data verbatim.

- ESP32-D0WD Tenstar TS0636G T-Display, advertised 16 MB flash and CH9102F USB bridge; classic LILYGO-compatible pinout is the starting profile.
- PlatformIO, ESP-IDF, C++; local Git first; MIT.
- P1 data to RX; external 10 kohm pull-up between RX and 3.3 V; RX polarity configurable in firmware.
- USB flashing/power OR meter 5 V power; never simultaneous.
- HDLC/DLMS/COSEM input, including segmented notifications from existing captures.
- 115200 8N1 inverted is the user-requested default matching the reference ESP8266 project (2026-10-26), NOT a verified electrical specification of the meter. Expose baud, parity, stop bits and inversion in full configuration.

## Wi-Fi and display

Unconfigured: setup AP plus DNS/HTTP captive portal containing Wi-Fi SSID/password only, scan/manual SSID support. Display AP SSID, setup IP, WLAN status and meter readings if present. Android/iOS captive portal detection is best effort; always show manual setup address. Never display passwords.

Configured: station mode; display IP and meter readings. Clarification: 'only IP' means no unnecessary WLAN configuration details; readings remain visible. Define reconnect behaviour with backoff; a temporary network outage must not erase credentials or silently open unrestricted setup.

The Full-access settings page provides a persistent 180° screen rotation option.
It defaults to off and takes effect on the next display refresh without reboot.

## Access and buttons

Persist None / View / Full. No application password. None blocks network UI/API, with acquisition, display and MQTT continuing. View permits live readings and read-only raw/decoded diagnostics. Full permits configuration and replay control too. Enforce on server, API and streaming connections, including permission changes mid-session.

Use right button GPIO35 as logical S1 unless actual board labels differ. Short press grants full access for 5 minutes, with remaining time on screen, then restores persisted policy. Long press resets Wi-Fi only after a visible countdown; choose and document threshold and suppress short-press action on release. Reboot cancels temporary elevation.

## Web, API and MQTT

Responsive offline-served live dashboard, timestamps, stale-data status, import/export, cumulative energy, phase voltage/current and known OBIS objects. No invented or zero-filled missing readings.

Configuration: broker host/IP, port, username/password, plain MQTT by default (port 1883) or explicitly enabled certificate-verified TLS, topic root, optional Home Assistant discovery. Default root openp1/<stable-device-id>. Publish UART data directly below the configured prefix; append `/simulation` for replay data, including separate state/raw/availability topics and Home Assistant identities. MQTT is allowed in both modes when enabled (user decision, 2026-10-25). Availability with retained LWT, state JSON and optional text topics. Retain discovery; do not retain streaming raw data by default.

Additional user-requested MQTT text format (2026-10-25): use the configured
connection/prefix and isolated normal/simulation branches, directly under the active source branch, alongside
the existing JSON schema. Publish original frame hex, reassembled LLC/APDU hex,
a configurable ID label and active/voltage/current/tariff summaries in the
supplied order. JSON /raw publication is removed; hex raw and hdlc follow text_raw_enabled independently of text_enabled measurement topics. Omit incomplete
or stale summary groups; derived voltage/current values are compatibility
outputs only, never invented transmitted meter objects.

User-requested MQTT simplification (2026-10-25): remove the debug console and
its capture/history/API. Show broker connection state, errors and a boot-lifetime
accepted-publication count in Settings. Show successfully decoded meter message
count on the main page, including simulation. Do not retain MQTT payload history.

## Diagnostics and replay

Show raw chunks, decoded scalar/string objects and errors side by side in a
read-only diagnostic page, with timestamps, raw numeric values, scalers/units and
unknown OBIS codes. Preserve raw bytes/unknown values in the shared parser.
Exclude credentials. State that meter identifiers can appear in diagnostics.

User scope reduction (2026-10-26): diagnostic export, capture upload/import,
copy/download and advanced replay pause/speed/loop-toggle are no longer required.
Keep the existing bounded simulation sequence editor, explicit HDLC versus
LLC/APDU layer, source selection and fixed 3.5-second repeat. Do not mix UART
and replay; publish replay under its simulation branch. The page uses only the
existing bounded raw tail and decoded snapshot; it does not promise a full
capture or historical error log.

User-requested runtime simulation settings (2026-10-25): Full-access settings
provide a persisted enable flag, explicit wire HDLC or LLC/APDU layer and editable
sequence. Fresh configuration defaults to disabled. Save and apply through a
visible device restart; UART and simulation never run simultaneously. Remove
simulation menuconfig options. Default to the existing meter APDU capture with
the serial/GR identifiers replaced by 12345678/1234567. Bound input to 4096 text
characters and 2048 decoded bytes. Repeat after a 3.5-second pause.

## Remaining validation, not software blockers

Exact meter manufacturer/model, electrical interface and actual serial settings; Tenstar pinout, flash size and GPIO35 external pull-up; meter power budget under Wi-Fi load; enclosure dimensions. Do not claim validated hardware support before bench tests.

User-requested MQTT topic layout (2026-10-25): remove `/text/` from all
compatibility topics. Publish `<prefix>/[simulation/]{id,active,voltage,
current,metering,raw,hdlc}` according to the independent switches. Optional
`obis_enabled` defaults off and publishes fresh attribute-2 values directly to
`<prefix>/<mode>/<display-OBIS>`, e.g. `1-0:51.7.0`. Apply transmitted scalers,
omit units in payloads, preserve text values and omit missing/stale/nonfinite
values. These messages are unretained QoS 0, on the state publication cadence.

User-requested state-topic switch (2026-10-25): independently enable/disable
MQTT JSON `/state` via a persisted boolean, default enabled to preserve existing
behavior. Other MQTT outputs and HTTP readings are unaffected. Home Assistant
state-based entities need `/state`; suppress new discovery publication while
state output is disabled, without deleting existing retained entity definitions.

User-requested access-policy save behavior (2026-10-25): a successful Full/CSRF
protected save displays an English restarting page and reboots the module.
Save failure keeps Settings open with an error and does not schedule a reboot.
Restart ends temporary grants; the persisted access policy is enforced at boot.

User-requested locked-browser page (2026-10-25): blocked main/settings browser
navigation shows English S1 instructions rather than JSON denial. The page waits
for Full access and returns to the main page automatically. A public static
waiting shell and status-only 204/403 probe are an explicit exception to the
None UI block; they expose no measurements/configuration/diagnostics or tokens.
Setup AP always refuses the probe. Protected APIs retain their access checks.

User-requested web overview (2026-10-25): show Wi-Fi signal quality below net grid power and above the
connection/source cards and a net-power gauge (1-0:1.7.0 minus 1-0:2.7.0), export
negative and import positive. Default range -5/+5 kW, adjustable in Full
Settings and persisted. Show Export/Import values in watts below. Explicit user
exception to missing-value rules: when export OBIS is absent, use 0 W only for
this web-derived gauge, with an explanatory label. Never overwrite parser/MQTT
readings or zero a stale/invalid export that was actually transmitted.

User topic-layout update (2026-10-26): remove the normal suffix entirely.
Real examples: `<prefix>/active`, `<prefix>/raw`, `<prefix>/state`,
`<prefix>/availability`, `<prefix>/1-0:51.7.0`. Replay retains its simulation
suffix. Preserve identities, credentials and independent publication switches.

## M6 web firmware update (2026-10-26)

User moves the former hardware/release M6 to M7. New M6 provides an application
.bin upload in Full Settings only. Enforce Full/origin/CSRF at entry and Full
throughout upload/final activation. Stream bounded chunks into an inactive OTA
slot; reject oversized, wrong-chip/project/date/header and invalid images. Only
select the boot slot after ESP-IDF checksum/hash/image validation. Preserve NVS
settings. Add date versions YYYY.MM.DD and a non-flashing script that builds and
packages the application-only .bin with checksum/metadata. One USB installation
is required to migrate the previous factory-only partition table; do not perform
it without explicit user request. Update/rollback accepted by user report on 2026-10-26; see M6.md for evidence limits.
