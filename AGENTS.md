# Development instructions

Read docs/REQUIREMENTS.md, docs/HARDWARE.md, docs/ARCHITECTURE.md and docs/ROADMAP.md first.

Use PlatformIO + ESP-IDF with C++17-compatible application code. Keep meter parsing portable and independent from ESP-IDF. Pin platform and added dependencies. Do not substitute Arduino without an explicit architectural decision.

This repository is a scaffold. Never describe planned features as implemented. Complete milestones in order; commit each working milestone and tag only after its stated checks pass. No public publishing or remote repository is configured. Never flash hardware unless the user requests it.

Preserve original raw bytes even when decoding fails. Bound every buffer and upload. Parse HDLC by declared length and protocol rules; do not split blindly on every 0x7E. Validate frame checksums, reassemble segments, then decode DLMS A-XDR and OBIS with scalers and units. Preserve unknown objects. A failed frame must not replace valid readings with zeros.

One shared pipeline for UART and replay. Reject malformed replay input clearly. Never silently convert application payloads into validated HDLC frames. Test malformed, fragmented, segmented and checksum-invalid inputs.

Enforce access policies server-side on every endpoint and existing streaming connection. No application password. Wi-Fi-only provisioning cannot expose configuration or diagnostics. S1 grants temporary full access for 300 seconds, then restores the persisted policy. MQTT continues under access=None.

No simultaneous meter and USB power. GPIO27 is proposed RX; GPIO16 is LCD DC. GPIO35 button needs an external pull-up (verify board resistor). Board and meter electrical compatibility remain unverified.

Keep secrets out of Git, logs, exports and API reads. For TLS verify broker certificates and hostname; never default to insecure TLS. Python arguments and return values must be typed. Document checks and limitations at each milestone.
