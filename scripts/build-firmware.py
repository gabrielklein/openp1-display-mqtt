#!/usr/bin/env python3
"""Build application OTA image and a ZIP with matching USB installation files."""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile


def date_version(value: str) -> str:
    try:
        parsed = dt.datetime.strptime(value, "%Y.%m.%d").date()
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Version must be a valid YYYY.MM.DD date") from exc
    if parsed.strftime("%Y.%m.%d") != value or parsed.year < 2000:
        raise argparse.ArgumentTypeError("Version must be a valid YYYY.MM.DD date from 2000 onward")
    return value


def inspect_image(data: bytes, expected_version: str) -> dict[str, str | int]:
    if len(data) <= 288 or len(data) > 0x300000 or data[0] != 0xE9 or not 1 <= data[1] <= 16:
        raise ValueError("Not an application image within the 3 MiB slot limit")
    if struct.unpack_from("<H", data, 12)[0] != 0 or data[23] != 1:
        raise ValueError("Expected classic ESP32 image with an appended SHA256 digest")
    if struct.unpack_from("<I", data, 32)[0] != 0xABCD5432:
        raise ValueError("Missing OpenP1 application descriptor")
    version = data[48:80].split(b"\0", 1)[0].decode("ascii")
    project = data[80:112].split(b"\0", 1)[0].decode("ascii")
    if project != "openp1_display" or version != expected_version:
        raise ValueError(f"Unexpected project/version: {project!r} / {version!r}")
    offset = 24
    for _ in range(data[1]):
        if offset + 8 > len(data):
            raise ValueError("Truncated segment header")
        length = struct.unpack_from("<I", data, offset + 4)[0]
        offset += 8 + length
        if offset > len(data):
            raise ValueError("Truncated image segment")
    digest_offset = (offset // 16 + 1) * 16
    if digest_offset + 32 != len(data) or hashlib.sha256(data[:digest_offset]).digest() != data[digest_offset:]:
        raise ValueError("Application image SHA256 is invalid or has trailing bytes")
    return {"version": version, "project": project, "chip": "ESP32", "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(), "type": "application-only OTA image"}



def write_upload_guide(output: Path, image_name: str, version: str) -> None:
    """Write a standalone guide used both in dist and inside the ZIP."""
    guide = f"""# OpenP1 Display — installation guide ({version})

Application image: **{image_name}**
Full installation kit: **openp1-display-{version}.zip**

## Choose an installation method

- Existing OTA-enabled device: upload the application .bin through its Settings.
- First installation/recovery: extract the ZIP and flash all four binaries over
  USB using esptool or Espressif's browser tool. No source checkout is required.
- Building from source: use PlatformIO upload instead.

Before USB, disconnect the ENTIRE meter cable. Never power from meter and USB
simultaneously. Confirm classic ESP32 with 16 MiB flash. Close any serial monitor.

## Device web updater

1. Open the device IP displayed on the LCD and obtain Full access. If enabled,
   a short S2 press grants Full for five minutes (lower-left button beside USB-C,
   with the display facing you and USB-C on the left).
2. Open Settings → Firmware update and select `{image_name}` only.
3. Upload, wait for validation/restart and check version, settings and MQTT.

Upload only the application .bin, NOT the ZIP, bootloader, partitions or metadata.
The maximum application size is 3 MiB. Settings are preserved. If the reply is lost,
check the installed version before retrying. Old factory-only layouts require one
full USB installation before using this updater.

## USB with esptool — extracted ZIP, no source required

Install Espressif esptool in a Python environment, then run from the extracted
ZIP directory. Replace /dev/ttyACM0 with your actual serial port (e.g. COM4).

For esptool v5:

```sh
python -m esptool --chip esp32 --port /dev/ttyACM0 write-flash --flash-mode dio --flash-freq 40m --flash-size 16MB 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 {image_name} 0x310000 ota_data_initial.bin
```

For esptool v4 (including the project's pinned PlatformIO tool):

```sh
python -m esptool --chip esp32 --port /dev/ttyACM0 write_flash --flash_mode dio --flash_freq 40m --flash_size 16MB 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 {image_name} 0x310000 ota_data_initial.bin
```

These commands install the factory application and reset OTA selection so it
boots. They do not erase NVS; do not request a whole-chip erase if you want to
preserve settings. Resetting OTA metadata clears existing boot/rollback selection.
Keep all files from the same kit and reset the device after flashing if needed.

### Application-only USB update (limited case)

If the matching bootloader/table are already installed AND factory boot is selected:

```sh
python -m esptool --chip esp32 --port /dev/ttyACM0 write-flash 0x10000 {image_name}
```

Use write_flash with esptool v4. The application offset is 0x10000, never 0x0 or
0x1000. After OTA use, factory-only flashing may leave the device booting the old
OTA slot. Use the device updater or the complete USB command above in that case.
Do not guess an OTA slot or overwrite a running slot.

## USB with PlatformIO — source checkout required

From the project source directory (not the extracted ZIP):

```sh
pio device list
pio run -e tenstar_tdisplay -t upload --upload-port /dev/ttyACM0
```

This builds and flashes the project's matching bootloader/table/OTA metadata/app.
It may build a different image/version from this downloaded kit. The project's
VERSION file controls the normal PlatformIO build version. The ZIP itself is
not a PlatformIO project; use esptool/browser flashing for these exact binaries.

## USB with the official Espressif browser tool

1. Extract the ZIP. Disconnect the meter and connect a USB data cable.
2. Open https://espressif.github.io/esptool-js/ in a Web Serial-capable desktop
   browser such as Chrome or Edge. The official tool is maintained at
   https://github.com/espressif/esptool-js .
3. Connect/select the board's serial port and allow browser access. If connection
   fails, close other serial applications and follow the board's download-mode
   procedure. S2 is the access button, not the GPIO0 boot/download button.
4. Add these four files from the same extracted kit at the exact addresses:

| Address | File |
| --- | --- |
| 0x1000 | bootloader.bin |
| 0x8000 | partitions.bin |
| 0x10000 | {image_name} |
| 0x310000 | ota_data_initial.bin |

5. Choose DIO flash mode, 40 MHz flash frequency and 16 MB flash size. Do not
   erase all flash if preserving NVS settings. Program and wait for completion.
6. Reset/disconnect-reconnect the board if needed, then check the LCD/version,
   Wi-Fi, stored settings, readings and MQTT.

This is generic USB flashing, separate from the module's own Wi-Fi update page.
Never upload the ZIP as a binary. Complete USB installation resets OTA selection.

## Package integrity and recovery

SHA256SUMS lists every binary's checksum. `sha256sum -c SHA256SUMS` checks the
extracted files on systems providing that command. The .json sidecar describes
the application. SHA256 verifies integrity, not publisher identity. Date versions
can be rebuilt; use the hash to identify an exact image.

Startup confirmation/rollback applies to OTA boots, not a newly written factory
image. Keep a USB recovery path. Further details in the source repository:
docs/FIRMWARE_UPDATE.md and hardware/HARDWARE.md. The build script never flashes
hardware or opens a serial device.
"""
    (output / "README.md").write_text(guide, encoding="utf-8")


def package_firmware(build: Path, output: Path, version: str) -> Path:
    """Package only named public artifacts from one completed PlatformIO build."""
    data = (build / "firmware.bin").read_bytes()
    info = inspect_image(data, version)
    support = {name: (build / name).read_bytes() for name in
               ("bootloader.bin", "partitions.bin", "ota_data_initial.bin")}
    if not support["bootloader.bin"] or support["bootloader.bin"][0] != 0xE9:
        raise ValueError("Missing or invalid ESP32 bootloader")
    table = support["partitions.bin"]
    entries: dict[str, tuple[int, int, int, int]] = {}
    for pos in range(0, len(table) - 31, 32):
        magic, kind, subtype, offset, size, label, _ = struct.unpack_from("<HBBII16sI", table, pos)
        if magic != 0x50AA:
            break
        entries[label.split(b"\0", 1)[0].decode("ascii")] = (kind, subtype, offset, size)
    expected = {"nvs": (1, 2, 0x9000, 0x6000), "factory": (0, 0, 0x10000, 0x300000),
                "otadata": (1, 0, 0x310000, 0x2000), "ota_0": (0, 0x10, 0x320000, 0x300000),
                "ota_1": (0, 0x11, 0x620000, 0x300000)}
    if any(entries.get(key) != value for key, value in expected.items()):
        raise ValueError("USB guide requires the project's matching 16 MiB OTA layout")
    if support["ota_data_initial.bin"] != b"\xff" * 8192:
        raise ValueError("Expected blank initial OTA metadata, not device-specific data")
    output.mkdir(parents=True, exist_ok=True)
    stem = f"openp1-display-{version}"
    image_name = stem + ".bin"
    files = {image_name: data, **support}
    files[stem + ".sha256"] = f"{info['sha256']}  {image_name}\n".encode()
    files[stem + ".json"] = (json.dumps(info, indent=2) + "\n").encode()
    files["SHA256SUMS"] = "".join(
        f"{hashlib.sha256(content).hexdigest()}  {name}\n"
        for name, content in files.items() if name.endswith(".bin")).encode()
    write_upload_guide(output, image_name, version)
    files["README.md"] = (output / "README.md").read_bytes()
    # Keep loose binaries for the device updater and manual browser selection.
    for name, content in files.items():
        (output / name).write_bytes(content)
    archive = output / (stem + ".zip")
    temporary = archive.with_suffix(".zip.tmp")
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
            for name, content in files.items():
                bundle.writestr(name, content)
        temporary.replace(archive)
    finally:
        temporary.unlink(missing_ok=True)
    return archive


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", type=date_version, default=(root / "VERSION").read_text().strip())
    parser.add_argument("--output-dir", type=Path, default=root / "dist")
    parser.add_argument("--build-dir", type=Path, help="Optional separate PlatformIO build directory")
    parser.add_argument("--pio", type=Path, help="Path to PlatformIO executable")
    args = parser.parse_args()
    pio = (args.pio or Path(shutil.which("pio") or Path.home() / ".platformio/penv/bin/pio")).resolve()
    if not pio.is_file():
        parser.error("PlatformIO not found; pass --pio /path/to/pio")
    build = (args.build_dir or root / ".pio/web-update" / args.version).resolve()
    env = os.environ.copy()
    env["PLATFORMIO_BUILD_DIR"] = str(build)
    env["OPENP1_BUILD_VERSION"] = args.version
    marker = build / ".openp1-version"
    previous = marker.read_text().strip() if marker.exists() else ""
    if (build / "tenstar_tdisplay/CMakeCache.txt").exists() and previous != args.version:
        subprocess.run([str(pio), "run", "-e", "tenstar_tdisplay", "-t", "clean"], cwd=root, env=env, check=True)
    # A version-specific build directory prevents cached descriptors from using another version.
    subprocess.run([str(pio), "run", "-e", "tenstar_tdisplay"], cwd=root, env=env, check=True)
    output = args.output_dir.resolve()
    archive = package_firmware(build / "tenstar_tdisplay", output, args.version)
    build.mkdir(parents=True, exist_ok=True)
    marker.write_text(args.version + "\n")
    print(f"Web-update image: {output / ('openp1-display-' + args.version + '.bin')}\n"
          f"USB installation ZIP: {archive}\nGuide: {output / 'README.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
