# Connectivity

network.cpp owns AP/STA lifecycle, Wi-Fi persistence, captive DNS, reconnect
backoff and S1. Shared guarded state exposes bounded snapshots without making
acquisition wait for network I/O. See ../../docs/ARCHITECTURE.md.
