# OpenP1 Display

Open-source P1 energy-meter reader for a Tenstar TS0636G, likely compatible with the classic LILYGO/TTGO T-Display ESP32-D0WD 16 MB CH9102F. MIT licence.

**Status: M5 accepted (local v0.5.0); M6 web firmware update in progress.**
M4 covers access/S1, MQTT/TLS/Home Assistant and the dashboard. M5 adds persisted
UART settings and read-only diagnostics. Both milestones are accepted; no public
publishing or agent flashing. Exact electrical
meter/board compatibility remains unverified. See [M4](docs/M4.md) and
[M5 scope and acceptance](docs/M5.md). Access defaults to Full; explicit policies
remain persisted. S1 firmware defaults off until its GPIO35 pull-up is verified.

## Continue with Codex

Open this folder in Codex and read `AGENTS.md`, `docs/REQUIREMENTS.md`, `docs/HARDWARE.md` and `docs/ROADMAP.md`. M5 is accepted; M6 implements web firmware update and awaits device acceptance (docs/M6.md). The proposed handoff prompt is in `docs/CODEX_HANDOFF.md`.

## Restore Git after extracting the ZIP

The ZIP includes a Git bundle with the initial commit and tag. To restore history, run from the directory containing the extracted folder:

```sh
git clone openp1-display/openp1-history.bundle openp1-work
cd openp1-work
```

The clone uses main. The bundle contains committed files; no public remote exists. Work in openp1-work. You can remove the local origin with `git remote remove origin`.

## Build

Install PlatformIO Core or the VS Code PlatformIO extension, then:

```sh
pio run -e tenstar_tdisplay
pio run -e tenstar_tdisplay -t upload
pio device monitor -b 115200
```

Disconnect the meter cable before connecting USB. Power from USB OR the meter, never both.

The PlatformIO platform is pinned to 6.9.0 as an initial reproducible baseline, not a claim that it is the latest version. See CHANGELOG.md for compilation checks. Confirm 16 MB flash using esptool before flashing. Review the custom partitions before use; storage is reserved but not mounted yet. Configure input and the proposed display profile through `pio run -e tenstar_tdisplay -t menuconfig`.

Upload fails during port detection with a Python version error? See
[build/upload troubleshooting](docs/BUILD_TROUBLESHOOTING.md).

## Host test

```sh
sh scripts/test-native.sh
node scripts/test-web.cjs
sh scripts/test-http.sh # requires the pinned local ESP-IDF package
```

## Layout

- `src/`: firmware and portable meter processing.
- `include/`: board profile and configuration model.
- `web/`: embedded setup/dashboard pages, no external CDN.
- `test/fixtures/`: anonymized replay regression inputs.
- `hardware/`: enclosure concept and future mechanical sources.
- `docs/`: requirements, architecture, API, hardware and milestones.
- `test/`: host and future integration tests.

The initial Git tag is `v0.0.1-scaffold`. Later tags must represent verified milestones, not empty releases.

In this workspace `.git` is an empty read-only mount. Local history is restored
from the supplied bundle in `.git-local`; use `git --git-dir=.git-local` for
status/log/commit commands. No remote is configured.

## Simulation settings

In the Full-access **Settings → Simulation** section, enable simulation, choose
the explicit HDLC or LLC/APDU input format and edit the hex sequence.
**Save simulation and restart** persists the configuration and restarts the
device; disabling it returns to UART after restart. No simulation build option
is required. A fresh installation defaults to UART with simulation disabled.

The default is the existing meter LLC/APDU capture with identifiers replaced
by `12345678` and `1234567`, preserving the other bytes and readings.
**Use default sequence** restores it. Input is limited to 4096 text characters
and 2048 raw bytes, with hex-only or `topic/with/slash hex` lines. No synthetic
HDLC frame or checksum is added. Chunks are 64 bytes at 20 ms intervals, with
a 3.5-second pause after each sequence. When enabled, MQTT uses the separate
`<prefix>/simulation/...` branch and simulation Home Assistant entities.

## MQTT text compatibility output

In Settings, **Publish text measurement topics** enables plain-text summaries
independently of JSON state and hexadecimal output. The broker, TLS,
credentials and saved topic prefix are shared with the existing MQTT connection.
For prefix `sensors/power/p1meter`, normal topics are
`sensors/power/p1meter/<name>`; simulation uses
`sensors/power/p1meter/simulation/<name>`.

| Name | Payload order / units |
| --- | --- |
| `raw` | Complete original HDLC frame as hex, including flags and checksums |
| `hdlc` | Reassembled LLC/APDU hex (the name follows the supplied format) |
| `id` | Configured text ID label, initially `ISK`; empty omits it |
| `active` | Import W, export W, import kWh, export kWh |
| `voltage` | Mean of L1–L3, L1, L2, L3, in V |
| `current` | Sum of L1–L3, L1, L2, L3, in A |
| `metering` | Import tariff 1/2, export tariff 1/2, in Wh |

Enable **Publish raw/HDLC hex topics (raw, hdlc)** for the `raw` and
`hdlc` hex topics. Direct LLC/APDU simulation emits no fabricated HDLC frames.
All text topics are QoS 0, non-retained. Summary groups require all relevant
fresh readings with matching timestamps and expected units; missing groups are
omitted. kWh totals and voltage/current summaries use two decimal places; exact
values/scalers/units remain in JSON. `ISK` is a user label, not detected hardware.
See docs/M4.md for best-effort diagnostic limits and the text-drop counter.

The allocation-recovery build requires C++ exceptions and a 2048-byte emergency
pool, with bounded Wi-Fi dynamic RX/TX buffers of 10/8. See
[build troubleshooting](docs/BUILD_TROUBLESHOOTING.md) for older saved profiles.
Debug history uses RAM only after enabling capture; stop and clear releases it.
Startup logs and guarded status APIs expose runtime heap statistics.

## Meter UART settings (M5)

Full Settings → Meter UART configures baud, parity, stop bits and RX inversion.
Save UART and restart persists these settings and displays the restarting page.
The default profile is 115200 8N1 with RX inversion, matching the reference
ESP8266 project. Menuconfig values are the initial fallback; saved settings take
precedence on later boots. UART1 remains GPIO27 with 8 data bits. Saving does not
turn simulation off; use Simulation settings to select the physical meter.
See docs/M5.md for revised diagnostics scope and recorded acceptance.

## Physical LCD overview

The color LCD shows net grid import/export in large W/kW digits: red import,
green export and amber stale data. L1/L2/L3 voltages sit below. The top row has
Wi-Fi strength arcs, MQ broker state (green connected, amber waiting, red error,
gray disabled) and an RX activity pulse for successfully decoded meter messages,
including simulation. The footer keeps IP/setup/S1 status. No extra framebuffer
is allocated; the eight-row DMA rendering and 180° setting remain in use.

## Read-only diagnostics

Open Diagnostics from the readings page or Settings. View and Full access show
raw chunks, decoded values and parser counters/latest error side by side. The
page updates every three seconds using bounded existing snapshots. It includes
meter identifiers, not connection credentials. The raw tail can start mid-frame;
raw/decoded panels are separate snapshots. On access loss it opens the S1 waiting
page. Export/import, copy/download and advanced replay controls were explicitly
removed from M5 scope; they are not planned features.

## Web firmware update (M6)

Firmware versions now use dates, initially **2026.10.26**. Build an uploadable
application image without flashing:

```sh
python3 scripts/build-firmware.py
# Optional version:
python3 scripts/build-firmware.py --version 2026.10.27
```

Upload `dist/openp1-display-<date>.bin` from **Full Settings → Firmware update**.
The script also writes checksum/metadata sidecars. View/None and setup AP cannot
update. The file is streamed into an inactive slot and validated before restart.

**One initial USB installation is required:** the old factory-only partition
layout cannot support web updates. Normal PlatformIO USB installation of this
revision installs the new OTA layout/bootloader. Disconnect the entire meter
cable before USB; never use both power sources together. No agent flashing was
done. See [M6 migration, rollback and acceptance](docs/M6.md). Former M6 hardware/
release work is now **M7**; date firmware versions are independent of milestone tags.
