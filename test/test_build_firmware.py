"""Host checks for the web-update packager; no device or network required."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
from pathlib import Path
import struct
from collections.abc import Callable

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("firmware_builder", root / "scripts/build-firmware.py")
assert spec is not None and spec.loader is not None
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def rejects(action: Callable[[], object]) -> None:
    try:
        action()
    except (ValueError, argparse.ArgumentTypeError):
        return
    raise AssertionError("Malformed input was accepted")


def main() -> None:
    assert builder.date_version("2028.02.29") == "2028.02.29"
    for version in ["2026.02.29", "2026.1.01", "1.0.0", "1999.12.31", "2026.13.01"]:
        rejects(lambda: builder.date_version(version))
    image = bytearray(304)
    image[0], image[1], image[23] = 0xE9, 1, 1
    struct.pack_into("<I", image, 28, 256)
    struct.pack_into("<I", image, 32, 0xABCD5432)
    image[48:58] = b"2026.10.26"
    image[80:94] = b"openp1_display"
    image.extend(hashlib.sha256(image).digest())
    info = builder.inspect_image(bytes(image), "2026.10.26")
    assert info["project"] == "openp1_display" and info["bytes"] == len(image)
    rejects(lambda: builder.inspect_image(bytes(image), "2026.10.27"))
    corrupt = image.copy()
    corrupt[150] ^= 1
    rejects(lambda: builder.inspect_image(bytes(corrupt), "2026.10.26"))
    rejects(lambda: builder.inspect_image(bytes(image) + b"extra", "2026.10.26"))
    rejects(lambda: builder.inspect_image(b"\0" * 4096 + bytes(image), "2026.10.26"))
    real = root / "dist/openp1-display-2026.10.26.bin"
    if real.exists():
        assert builder.inspect_image(real.read_bytes(), "2026.10.26")["bytes"] > 100000
    print("Firmware packager date/image/SHA256 tests passed")


if __name__ == "__main__":
    main()
