#!/usr/bin/env python3
"""Build and package the application-only ESP32 image for the Full-access web updater."""
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
    """Write installation guidance beside the application-only package."""
    guide = f"""# OpenP1 Display — flashing guide

Latest packaged version: **{version}**
Application image: **{image_name}**

## Update through the device (recommended for an existing OTA installation)

1. Keep the device powered and connected to Wi-Fi; open its displayed IP address.
2. Obtain Full access. If enabled, a short S2 press grants it for five minutes.
   S2 is the lower-left button next to USB-C, with the USB port on the left.
3. Open Settings → Firmware update and select `{image_name}`.
4. Upload and wait for validation/restart without interrupting power.
5. Check the installed version, settings, meter readings and MQTT afterward.

Upload the .bin only. The .sha256 and .json files are checksum/metadata sidecars.
Maximum application size: 3 MiB. Settings are preserved. If the upload reply is
lost, check the installed version before retrying. This application-only image
cannot install the bootloader or migrate an old factory-only partition table.

## First installation or recovery over USB

Disconnect the ENTIRE meter cable before USB. Never power from meter and USB
simultaneously. Confirm the board has 16 MiB flash and close any serial monitor.

From the project source directory, the recommended complete USB installation is:

```sh
pio run -e tenstar_tdisplay -t upload --upload-port /dev/ttyUSB0
```

Replace the port with your actual board port. This builds/installs the matching
bootloader, partition table, initial OTA metadata and factory application.
A source-less first installation needs those matching binaries as well as this
application; this dist directory is not a complete first-installation kit.

## Official Espressif browser tool (USB, not the device web updater)

Espressif maintains ESP Tool / esptool-js:
https://espressif.github.io/esptool-js/
Source and browser requirements: https://github.com/espressif/esptool-js

Use a supported Web Serial browser such as desktop Chrome or Edge. Connect the
USB serial port and select files with explicit flash addresses. For a complete
installation of THIS project's current classic-ESP32 16 MiB layout, the matching
PlatformIO build outputs are:

| Address | File from the same tenstar_tdisplay build |
| --- | --- |
| 0x1000 | bootloader.bin |
| 0x8000 | partitions.bin |
| 0x310000 | ota_data_initial.bin |
| 0x10000 | {image_name} (application / firmware.bin) |

Use the project's matching build outputs and flash parameters; do not mix
versions/layouts. For the application use 0x10000, never 0x0 or 0x1000. Do not
manually overwrite a running OTA slot. After prior OTA use, writing the factory
application alone may not change which slot boots: prefer the device updater,
or a deliberate full USB recovery with the matching initial OTA metadata.
Do not erase all flash if you want to preserve NVS settings. Resetting OTA
metadata clears the selected OTA boot/rollback state. The browser tool is a
manual generic flasher, not an OpenP1-specific one-click installer.

For detailed instructions in the project see docs/FIRMWARE_UPDATE.md and
hardware/HARDWARE.md. Date versions are not unique build IDs; compare SHA256
when identifying an exact file. No upload or device access is performed by the
build/package script.
"""
    (output / "README.md").write_text(guide, encoding="utf-8")


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
    source = build / "tenstar_tdisplay/firmware.bin"
    data = source.read_bytes()
    info = inspect_image(data, args.version)
    build.mkdir(parents=True, exist_ok=True)
    marker.write_text(args.version + "\n")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    target = output / f"openp1-display-{args.version}.bin"
    target.write_bytes(data)
    target.with_suffix(".sha256").write_text(f"{info['sha256']}  {target.name}\n")
    target.with_suffix(".json").write_text(json.dumps(info, indent=2) + "\n")
    write_upload_guide(output, target.name, args.version)
    print(f"Web-update image: {target}\nVersion: {args.version}\nSHA256: {info['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
