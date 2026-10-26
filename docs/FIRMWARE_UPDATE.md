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
`dist/README.md` with device-update and USB flashing instructions. Use `--pio` for a specific
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
were not supplied. See TEST_PLAN.md for remaining hardware/release validation.

## Official browser USB flasher

Espressif's [ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/)
is a generic serial flasher that runs in a supported browser, such as desktop
Chrome or Edge. See its [official repository](https://github.com/espressif/esptool-js).
It is separate from the device's own firmware-update page. The generated
`dist/README.md` lists the current project's file offsets and prerequisites.
The application-only package is not a complete first-installation kit; full USB
installation also needs matching bootloader, partitions and OTA metadata.
