#!/bin/sh
# Build/package only; never flashes hardware. Extra arguments go to the builder.
set -eu
cd "$(dirname "$0")"
exec python3 scripts/build-firmware.py "$@"
