# OpenP1 Display

OpenP1 Display is an ESP32 device that reads electricity-meter data through the
P1 port and shows how much power you are importing from or exporting to the grid.
A color screen gives an at-a-glance view, while Wi-Fi connects the readings to a
local web dashboard and your home automation system.

- **Live color display:** large import/export power, three-phase voltages,
  Wi-Fi strength, MQTT status and a receive activity indicator.
- **Local web dashboard:** net-power gauge, phase readings, energy totals,
  connection status and read-only raw/decoded diagnostics.
- **MQTT and Home Assistant:** configurable topic prefix, optional Discovery,
  JSON state, compact text summaries, individual OBIS values and raw/HDLC hex.
  Optional TLS verifies the broker certificate and hostname.
- **Web firmware upgrades:** upload an application `.bin` from Full-access
  Settings, with image validation, date-based versions and startup rollback.
- **Browser configuration:** Wi-Fi provisioning, persistent UART/MQTT settings,
  access policies, screen rotation and configurable power-gauge range.
- **Simulation mode:** replay an editable anonymized sequence through the same
  parser, with MQTT data isolated under `/simulation`.

![OpenP1 Display connected to a meter P1 port, showing live import power and three-phase voltages](hardware/LiveDevice.jpg)

Built with PlatformIO, pinned `espressif32@6.9.0`, ESP-IDF 5.3.1 and
C++17-compatible application code. MIT licence.

The current software through M6 is implemented and accepted by user report.
M7 was closed by explicit user acceptance on 2026-10-29; see the
[acceptance record](docs/M7_ACCEPTANCE.md) for evidence and limitations.
The proposed board is a Tenstar TS0636G T-Display with 16 MiB flash; exact
board/meter electrical compatibility still requires verification.

[Buy the T-Display board on AliExpress](https://s.click.aliexpress.com/e/_EzsGsy2)
(user-provided purchase link).

## Development note

The code for this project was written by OpenAI Codex, guided by the project
owner’s requirements, feedback and on-device testing.

## Documentation

Start with the **[user guide](docs/USER_GUIDE.md)** for installation, Wi-Fi setup,
access buttons, Settings, MQTT, simulation, updates and troubleshooting.

Find installation-specific UART settings in the **[country and meter profiles](docs/countries/README.md)**,
starting with [Switzerland / Romande Energie — ISKRAEMECO AM550](docs/countries/switzerland/romande-energie.md).

## Why I built this

I started with an existing ESP8266 P1-to-MQTT project, but I could not get it
working with my setup. I also wanted the readings directly on the device:
a clear view of import/export power and phase voltages without opening a
dashboard or checking MQTT messages.

That led to OpenP1 Display: a version built around the ESP32 T-Display, with a
local color screen, configurable MQTT outputs, a browser-based interface and
web firmware updates. The compact MQTT summary format remains compatible with
the previous setup. This reflects my experience with my hardware and meter;
it is not a claim that the original project fails for other users.

## Alternative projects

- **[AMS reader firmware](https://github.com/UtilitechAS/amsreader-firmware):**
  another meter-reader firmware project with publicly available source. Check its supported hardware
  and meter formats against your installation.
- **[ESPHome DSMR component](https://esphome.io/components/sensor/dsmr/):**
  an alternative for reading supported DSMR meters within an ESPHome setup.
  A P1 connector alone does not establish protocol compatibility with this
  project's HDLC/DLMS input.

These are related projects to evaluate, not a tested compatibility ranking.
The reason for this implementation was the combination of an on-device display
and a working solution for my particular setup.

## Bill of materials (BOM)

| Item                           | Quantity  | Specification / purchase link                                                                     |
| ------------------------------ | --------- | ------------------------------------------------------------------------------------------------- |
| ESP32 T-Display board          | 1         | [Tenstar TS0636G T-Display](https://s.click.aliexpress.com/e/_EzsGsy2), 16 MiB flash profile      |
| P1 connector/cable             | 1         | [RJ12 6P6C](https://s.click.aliexpress.com/e/_EJhce4K), six positions and six contacts            |
| RX pull-up resistor            | 1         | **10 kΩ**, between GPIO27 (RX) and 3.3 V, for the proposed compatible open-collector interface    |
| Insulated wire and heat-shrink | As needed | Secure, insulated connections                                                                     |
| Nonconductive enclosure        | 1         | Sized for the board, display, button and cable strain relief. Mine was provided with the tenstar. |

Purchase links were supplied by the project owner. Verify the actual board,
connector pin mapping and meter electrical interface before assembly. A pull-up
resistor does not make a push-pull 5 V signal safe for the ESP32. GPIO35 may also
need an external button pull-up if the board does not provide one; verify it
before enabling the access button. See [hardware](hardware/HARDWARE.md).

**Disconnect the entire meter cable before connecting USB; never power from both.**

## Build and install

```sh
pio run -e tenstar_tdisplay
```

For an initial installation or migration from the old factory-only partition
layout, install the bootloader, partition table and firmware by USB:

```sh
pio run -e tenstar_tdisplay -t upload --upload-port /dev/ttyUSB0
pio device monitor -b 115200
```

Replace the example port with the actual device. **Disconnect the entire meter
cable before connecting USB. Never power from USB and the meter simultaneously.**
Confirm 16 MiB flash before using this partition layout. See
[hardware](hardware/HARDWARE.md) and [troubleshooting](docs/BUILD_TROUBLESHOOTING.md).

To build only an uploadable application package, without flashing:

```sh
python3 scripts/build-firmware.py
# Optional date version:
python3 scripts/build-firmware.py --version 2026.10.27
```

The script writes `dist/openp1-display-<date>.bin`, `.sha256` and `.json`,
plus a complete `dist/openp1-display-<date>.zip` USB installation kit and
`dist/README.md` explaining esptool, PlatformIO, browser flashing and device updates.
Upload only the application `.bin` through **Full Settings → Firmware update**.
See [firmware updates](docs/FIRMWARE_UPDATE.md) for migration and rollback.

## First use and access

An unconfigured device opens a Wi-Fi setup AP and captive portal. Use the
address shown on the LCD if the portal does not open automatically. This portal
configures Wi-Fi only. After connection, open the station IP shown on the LCD.

The persisted access policy defaults to **Full** on a fresh configuration:

- **None:** network readings/settings blocked; acquisition, LCD and MQTT continue.
- **View:** dashboard and read-only diagnostics.
- **Full:** readings, settings and firmware update.

There is no application password; use the existing trusted-LAN access model.
**S2 is the lower-left button next to the USB-C port**, with the display facing
you and the USB-C port on the left (as in the [front photo](hardware/Front.jpg)).
Press and release it to grant Full access for 300 seconds; an eight-second hold
resets Wi-Fi only. This is the GPIO35 access button, called **S2** in firmware settings and
on-screen instructions. It defaults off in menuconfig until
its pull-up is verified. Locked browser pages return to the dashboard once Full
is available.
Saving the access policy restarts the device and cancels any temporary grant.

## Readings and configuration

The color LCD shows large net import/export power, phase voltages, Wi-Fi strength,
MQTT state and successful-decode activity. Import is red, export green and stale
data amber. Full Settings provides 180° rotation and a web gauge range, initially
±5 kW. The dashboard also shows Wi-Fi signal and messages received since boot.
When export is absent, the derived gauge/LCD can assume zero export; this never
changes parser or MQTT readings. Present invalid/stale readings are not zeroed.

UART defaults to **115200 8N1 inverted**, RX GPIO27. Full Settings can persist
baud, parity, stop bits and inversion; changes apply after restart. The default
is a software profile, not a verified electrical specification.

Simulation is disabled on a fresh configuration. Full Settings selects UART or
simulation, explicit wire HDLC versus LLC/APDU input, and a bounded hex sequence.
Saving restarts the device. Simulation repeats after 3.5 seconds and uses the same
parser as UART. Its default identifiers are `12345678` and `1234567`.

[MQTT configuration and topic formats](docs/MQTT.md) covers independent JSON,
summary, hex and OBIS outputs. Real topics are directly under the configured
prefix; simulation adds `/simulation`. A leading `/` is allowed and preserved.
Settings reports broker state, errors and accepted-publication count; there is
no MQTT debug console or payload history.

Diagnostics shows a bounded raw tail, decoded values and latest parser error.
It is read-only, available to View/Full, and can contain real meter identifiers.
Capture import/export/download and advanced replay controls are not provided.

## Checks

```sh
sh scripts/test-native.sh
node scripts/test-web.cjs
sh scripts/test-http.sh
python3 test/test_build_firmware.py
```

HTTP tests require the pinned local ESP-IDF package (or `IDF_PATH`). Host doubles
verify control flow; they do not replace hardware tests. See the
[M7 test plan](docs/TEST_PLAN.md) and [roadmap](docs/ROADMAP.md).

## Documentation and source

- [Requirements](docs/REQUIREMENTS.md), [architecture](docs/ARCHITECTURE.md),
  [API](docs/API.md) and [simulation format](docs/CAPTURE_FORMAT.md).
- `src/`: firmware, portable meter parser, network, display and MQTT.
- `web/`: embedded pages, served offline without a CDN.
- `test/fixtures/`: anonymized regression fixtures with valid adjusted checksums.
- `hardware/`: enclosure concept; dimensions and CAD remain unvalidated.
- `docs/pre-release/`: historical development notes, not current instructions.

Local Git history was reset to an anonymized snapshot; old milestone commits,
tags and the historical bundle are absent. In this workspace the writable Git
directory is `.git-local`; use `git --git-dir=.git-local --work-tree=.` before
Git subcommands. No remote is configured. See [handoff](docs/CODEX_HANDOFF.md).
