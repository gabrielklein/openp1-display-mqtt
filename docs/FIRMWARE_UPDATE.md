# Web firmware updates

Firmware versions use valid calendar dates `YYYY.MM.DD`, read from `VERSION`
(currently 2026.10.26). The actual built version appears in the ESP-IDF descriptor,
boot log, status API and Settings. Date versions are independent of Git history.

## Build the application package

```sh
python3 scripts/build-firmware.py
python3 scripts/build-firmware.py --version 2026.10.27
```

The script builds the pinned PlatformIO environment without upload or serial access.
It verifies chip/project/version, image bounds and appended SHA256, then writes
`dist/openp1-display-<date>.bin`, `.sha256` and `.json`, plus a generated
`dist/README.md` with device-update and USB flashing instructions. It also creates
`dist/openp1-display-<date>.zip` containing the application, matching bootloader,
partition table, blank initial OTA metadata, guide, sidecars and `SHA256SUMS`.
Loose copies are available in dist as well. Each ZIP is self-contained for USB
installation on the supported layout; only its application .bin is used for OTA. Use `--pio` for a specific
PlatformIO executable or `--build-dir` for a custom build directory. Date-specific
build directories and a version marker avoid stale descriptor versions.

Same-date rebuilds are allowed; the checksum identifies an exact image. The date
does not enforce upgrade order: downgrades are allowed and eFuse anti-rollback is
not configured. These images are not publisher-signed; SHA256 checks integrity,
not identity. Use the existing trusted-LAN/Full access model.

## Initial USB installation

Old factory-only installations cannot web-migrate their partition table. One USB
installation of the bootloader, table, initial OTA metadata and app is required.
Disconnect the complete meter cable before USB; never use both power sources.
Use the normal PlatformIO upload with the actual port. Confirm 16 MiB capacity.

| Partition | Offset | Size |
| --- | --- | --- |
| NVS | 0x9000 | 0x6000 |
| PHY | 0xf000 | 0x1000 |
| Factory app | 0x10000 | 0x300000 |
| OTA metadata | 0x310000 | 0x2000 |
| OTA slot 0 | 0x320000 | 0x300000 |
| OTA slot 1 | 0x620000 | 0x300000 |
| Reserved unmounted storage | 0x920000 | 0x6e0000 |

NVS addresses are unchanged. The OTA layout repurposes part of previously unused
storage; it does not migrate any external/custom filesystem data.

## Upload and restart

1. Open **Settings → Firmware update** with Full access on the station network.
2. Choose only `dist/openp1-display-<date>.bin`, not a merged image, bootloader,
   partition table, checksum or manifest.
3. Keep power stable while upload/validation runs and the device restarts.
4. Reopen the dashboard and check version, settings, readings and MQTT.

The upload limit is 3 MiB and 180 seconds. It streams into an inactive OTA slot,
validates classic ESP32/project/date/header and SDK image integrity, then selects
that slot and schedules restart. Repeated access checks abort on loss of Full;
transport/write/validation failures do not select a partial image. Another upload
is blocked while restart or startup verification is pending. Polling is paused
and writes are not automatically retried. If the success reply is lost after boot
selection, restart still proceeds: check the installed version before retrying.
Flash writes may add acquisition latency; continuous meter sampling is not promised.

## Startup confirmation and rollback

Bootloader rollback must be enabled. Existing saved profiles may need
**Bootloader config → Enable app rollback support** in menuconfig; compilation
rejects a disabled profile. An OTA app is confirmed after 30 seconds when network/
HTTP initialization succeeded and acquisition reaches its update loop. Incoming
meter data or a broker connection is not required. Failed initialization requests
rollback; reset/crash before confirmation invokes bootloader rollback. Later
arbitrary functional faults are not detected automatically. Keep a USB recovery path.

The user accepted the migration/update/version/settings/MQTT, invalid-image and
rollback checklist. Exact flashed hash and detailed fault-injection procedures
were not supplied. M7 was subsequently accepted on 2026-10-29; see M7_ACCEPTANCE.md for evidence
and limitations, and TEST_PLAN.md for repeatable validation.

## Official browser USB flasher

Espressif's [ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/)
is a generic serial flasher that runs in a supported browser, such as desktop
Chrome or Edge. See its [official repository](https://github.com/espressif/esptool-js).
It is separate from the device's own firmware-update page. The generated
`dist/README.md` lists the current project's file offsets and prerequisites.
The versioned ZIP includes the matching first-installation files. Extract it,
connect the USB serial port, and add these four files in the browser tool:

| Address | File |
| --- | --- |
| 0x1000 | bootloader.bin |
| 0x8000 | partitions.bin |
| 0x10000 | openp1-display-<date>.bin |
| 0x310000 | ota_data_initial.bin |

Select DIO, 40 MHz and 16 MB, program all four files, then reset and check the
installed version. Close serial monitors first. Do not erase all flash when
preserving NVS settings. Full USB installation resets the OTA boot/rollback
selection to factory. Never upload the ZIP as a single flash image.

## Direct USB with esptool

From the extracted ZIP directory, replace the port and date below. With esptool v5:

```sh
python -m esptool --chip esp32 --port /dev/ttyACM0 write-flash --flash-mode dio --flash-freq 40m --flash-size 16MB 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 openp1-display-2026.10.26.bin 0x310000 ota_data_initial.bin
```

With esptool v4 use `write_flash`, `--flash_mode`, `--flash_freq` and `--flash_size`.
The pinned PlatformIO tool is v4. No source checkout is required for these binaries.
For a factory-boot device with the correct bootloader/table already installed,
application-only flashing can use `write-flash 0x10000 <application.bin>` instead.
After OTA use, that alone may leave the old OTA slot selected; prefer the device
updater or complete USB installation. Never flash this application at 0x0/0x1000.

## PlatformIO USB upload

From a source checkout, not the extracted ZIP:

```sh
pio device list
pio run -e tenstar_tdisplay -t upload --upload-port /dev/ttyACM0
```

This builds and installs the matching bootloader/table/metadata/application. It
uses the checkout's VERSION and configuration, so it can differ from a downloaded
ZIP. To install the ZIP's exact binaries use esptool or the browser tool above.
Disconnect the complete meter cable before USB for every method.
