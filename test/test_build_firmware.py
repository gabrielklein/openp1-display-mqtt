"""Host checks for the web-update packager; no device or network required."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import zipfile
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
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        build, output = work / "build", work / "dist"
        build.mkdir()
        (build / "firmware.bin").write_bytes(image)
        (build / "bootloader.bin").write_bytes(b"\xe9test-bootloader")
        entries = [("nvs", 1, 2, 0x9000, 0x6000), ("factory", 0, 0, 0x10000, 0x300000),
                   ("otadata", 1, 0, 0x310000, 8192), ("ota_0", 0, 0x10, 0x320000, 0x300000),
                   ("ota_1", 0, 0x11, 0x620000, 0x300000)]
        table = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size,
                                     name.encode(), 0) for name, kind, subtype, offset, size in entries)
        (build / "partitions.bin").write_bytes(table)
        (build / "ota_data_initial.bin").write_bytes(b"\xff" * 8192)
        (build / "private-settings.txt").write_text("must not enter the archive")
        archive = builder.package_firmware(build, output, "2026.10.26")
        with zipfile.ZipFile(archive) as bundle:
            assert bundle.testzip() is None
            expected = {"README.md", "SHA256SUMS", "bootloader.bin", "partitions.bin",
                        "ota_data_initial.bin", "openp1-display-2026.10.26.bin",
                        "openp1-display-2026.10.26.json", "openp1-display-2026.10.26.sha256"}
            assert set(bundle.namelist()) == expected
            for name in expected:
                assert bundle.read(name) == (output / name).read_bytes()
            for line in bundle.read("SHA256SUMS").decode().splitlines():
                digest, name = line.split()
                assert hashlib.sha256(bundle.read(name)).hexdigest() == digest
            guide = bundle.read("README.md").decode()
            for instruction in ["write_flash", "write-flash", "pio run", "0x10000", "0x310000",
                                "https://espressif.github.io/esptool-js/", "Settings → Firmware update"]:
                assert instruction in guide
        (build / "ota_data_initial.bin").write_bytes(b"\0" * 8192)
        rejects(lambda: builder.package_firmware(build, work / "invalid-metadata", "2026.10.26"))
        (build / "ota_data_initial.bin").write_bytes(b"\xff" * 8192)
        (build / "partitions.bin").write_bytes(b"wrong-layout")
        rejects(lambda: builder.package_firmware(build, work / "invalid-layout", "2026.10.26"))
    real = root / "dist/openp1-display-2026.10.26.bin"
    if real.exists():
        assert builder.inspect_image(real.read_bytes(), "2026.10.26")["bytes"] > 100000
    print("Firmware packager date/image/SHA256/ZIP/layout/guide tests passed")


if __name__ == "__main__":
    main()
