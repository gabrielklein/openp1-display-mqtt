#!/bin/sh
# Flash the latest complete dist package over USB; does not rebuild firmware.
set -eu
cd "$(dirname "$0")"
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 SERIAL_PORT [--dry-run]" >&2
    echo "Example: $0 /dev/ttyACM0" >&2
    exit 2
fi
exec python3 - "$@" <<'PY'
from __future__ import annotations
import hashlib
import importlib.util
from pathlib import Path
import shlex
import subprocess
import sys


def main() -> int:
    port = sys.argv[1]
    dry_run = len(sys.argv) == 3 and sys.argv[2] == "--dry-run"
    if not port or port.startswith("-") or (len(sys.argv) == 3 and not dry_run):
        raise ValueError("Provide a serial port and optionally --dry-run")
    output = Path("dist")
    checksums = output / "SHA256SUMS"
    if not checksums.is_file():
        raise ValueError("No complete package found. Run ./create-dist.sh first.")
    files: dict[str, Path] = {}
    for line in checksums.read_text().splitlines():
        digest, name = line.split()
        if name != Path(name).name or name in files:
            raise ValueError("Invalid or duplicate package filename")
        path = output / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Checksum mismatch: {name}; rebuild the package")
        files[name] = path
    applications = [name for name in files if name.startswith("openp1-display-") and name.endswith(".bin")]
    if len(applications) != 1 or set(files) != {
        applications[0], "bootloader.bin", "partitions.bin", "ota_data_initial.bin"
    }:
        raise ValueError("Expected one application and its three matching USB support files")
    if files["ota_data_initial.bin"].read_bytes() != b"\xff" * 8192:
        raise ValueError("Invalid initial OTA metadata")
    if importlib.util.find_spec("esptool") is not None:
        tool = [sys.executable, "-m", "esptool"]
    else:
        core = Path.home() / ".platformio"
        interpreter = core / "penv/bin/python"
        script = core / "packages/tool-esptoolpy/esptool.py"
        if not interpreter.is_file() or not script.is_file():
            raise ValueError("Install esptool for python3 or the project's PlatformIO tools first")
        tool = [str(interpreter), str(script)]
    # esptool v4 uses underscores; v5 uses hyphens.
    help_text = subprocess.check_output(tool + ["--help"], text=True)
    separator = "-" if "write-flash" in help_text else "_"
    command = tool + ["--chip", "esp32", "--port", port, f"write{separator}flash",
                      f"--flash{separator}mode", "dio", f"--flash{separator}freq", "40m",
                      f"--flash{separator}size", "16MB"]
    for address, name in [("0x1000", "bootloader.bin"), ("0x8000", "partitions.bin"),
                          ("0x10000", applications[0]), ("0x310000", "ota_data_initial.bin")]:
        command.extend([address, str(files[name])])
    print("Disconnect the ENTIRE meter cable before USB. Never power from both sources.", flush=True)
    print("Close serial monitors. Target: classic ESP32, 16 MiB flash.", flush=True)
    print("Flashes the factory app and resets OTA selection; no whole-chip erase.", flush=True)
    print(shlex.join(command), flush=True)
    if dry_run:
        print("Dry run: no serial device opened or flashed.")
        return 0
    return subprocess.run(command, check=True).returncode


try:
    raise SystemExit(main())
except (ValueError, OSError, subprocess.CalledProcessError) as error:
    print(f"Flash failed: {error}", file=sys.stderr)
    raise SystemExit(1)
PY
