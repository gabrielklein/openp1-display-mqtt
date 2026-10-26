# Build and upload troubleshooting

## Upload stops at port detection with a Python 3.14 error

Observed locally: firmware compilation with the pinned
`espressif32@6.9.0` / ESP-IDF 5.3.1 succeeds, then upload stops at
`Looking for upload port...` with `Python version must be between 3.10 and 3.13`.

Local source inspection identifies the check in a different installed platform:
`~/.platformio/platforms/espressif32/platform.py`, whose platform.json identifies
pioarduino 55.03.35. The pinned official 6.9.0 platform has no such check.
PlatformIO's `SerialPortFinder._find_known_device()` may instantiate all installed
platforms while collecting board USB identifiers; the unrelated platform's
SystemExit aborts discovery. This error alone does not mean the firmware failed
to compile or that the project's ESP-IDF architecture must change.

Select the actual port explicitly, bypassing that global discovery:

```sh
platformio device list
```

Disconnect the complete meter cable before connecting USB. Identify the USB
bridge for this board; prefer its stable path under `/dev/serial/by-id/` when
available. If the board is listed as `/dev/ttyUSB0`, for example, run:

```sh
platformio run -e tenstar_tdisplay --target upload --upload-port /dev/ttyUSB0
```

Use the actual listed port, not an arbitrary ttyUSB number or a broad wildcard.
To use the regular VS Code Upload task, set the actual device path locally under
`[env:tenstar_tdisplay]` in platformio.ini:

```ini
upload_port = /dev/serial/by-id/ACTUAL-BOARD-IDENTIFIER
```

Replace the placeholder; do not commit machine-specific port settings. The
alternative `PLATFORMIO_UPLOAD_PORT` environment variable is also supported,
but VS Code must inherit that variable to use it for its tasks. See the
[official upload_port documentation](https://docs.platformio.org/en/latest/projectconf/sections/env/options/upload/upload_port.html).

A targeted host check against the installed PlatformIO finder verified that
an explicit non-wildcard port returns without loading other platforms and
without opening a serial device. No serial device is exposed in the assistant's
environment; no upload was attempted or confirmed. If a later step reports
another error, retain its full log: this workaround addresses this discovery
failure only. M2 physical display acceptance remains pending.

For broader environment compatibility, an isolated PlatformIO installation using
Python 3.12 is an alternative, but no global Python or installed platform was
modified for this workaround. Python 3.12.12 is already installed locally at
`~/.pyenv/versions/3.12.12/bin/python`; the `python3.12` shim is not currently
selected. Do not remove global environments or upgrade the pinned project
platform merely to bypass the discovery issue.

## Browser reports “Header fields are too long” (HTTP 431)

ESP-IDF defaults to a 512-byte request-header limit, which can reject browser
requests before the setup/scan handlers run. The project defaults now use a
bounded 4096-byte limit (`CONFIG_HTTPD_MAX_REQ_HDR_LEN`). Request-body and
credential bounds are unchanged.

Existing generated `sdkconfig.tenstar_tdisplay` files keep their previous
values when `sdkconfig.defaults` changes. If your local configuration still
uses 512, run `pio run -e tenstar_tdisplay -t menuconfig`, open
**Component config → HTTP Server → Max HTTP Request Header Length**, set
**4096**, save, rebuild and upload. Do not delete the entire configuration:
that would also discard replay/serial settings. Disconnect the complete meter
cable before USB. Requests exceeding 4096 bytes remain rejected; if necessary,
try a private browser window to exclude accumulated cookies for the device IP.

## Allocation recovery profile

After enabling C++ exceptions, an incremental build may fail with
`undefined reference to __eh_frame` from `startup.c.o`. The pinned PlatformIO
builder can retain a linker script preprocessed with the previous configuration,
while recompiling startup with exceptions enabled. Clean generated build files
and rebuild, keeping the saved board/serial settings:

```sh
pio run -e tenstar_tdisplay -t clean
pio run -e tenstar_tdisplay
```

Do not delete `sdkconfig.tenstar_tdisplay` or define a dummy `__eh_frame` symbol:
the linker must retain the actual exception unwind table.

The application now requires CONFIG_COMPILER_CXX_EXCEPTIONS=y and
CONFIG_COMPILER_CXX_EXCEPTIONS_EMG_POOL_SIZE=2048 or larger. Defaults also cap
Wi-Fi dynamic RX/TX buffers at 10/8. sdkconfig.defaults does not override an
existing saved profile: update older profiles with `pio run -e tenstar_tdisplay
-t menuconfig`, preserving other board/serial/S1 settings. main.cpp rejects the
old no-exceptions/zero-pool profile.

Startup logs show free/largest 8-bit heap; guarded status/values APIs expose
free_heap_bytes, largest_free_block_bytes and minimum_free_heap_bytes. Static
RAM percentages alone do not establish runtime headroom. Physical soak testing
remains required.
