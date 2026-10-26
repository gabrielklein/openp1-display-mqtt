# Milestones

| Milestone / target tag | Deliverable | Acceptance |
| --- | --- | --- |
| M0 / v0.0.1-scaffold | Structure, contracts, board profile, MIT, Git | Host hex tests; scaffold limitations explicit |
| M1 / v0.1.0 | UART/inversion, bounded raw capture, replay input | ESP build; replay chunking and UART settings tests |
| M2 / v0.2.0 | HDLC checksums/reassembly, DLMS/OBIS, display | Complete real capture tests plus bad CRC, truncation, fragmentation, unknown objects; physical display test |
| M3 / v0.3.0 | Wi-Fi provisioning, dashboard and read API | Android/iOS captive portal best-effort tests; reconnect; stale readings |
| M4 / v0.4.0 | Access guards, S1, MQTT/TLS/discovery | None/View/Full matrix; expiry on open streams; TLS rejection; broker recovery |
| M5 / v0.5.0 | Persisted configuration and read-only raw/decoded/error diagnostics | Access guards; bounded memory; no secrets; normal/simulation MQTT isolation; user acceptance |
| M6 / v0.6.0 | Full-only web firmware update, date version and build script | Bounded validated uploads; access/expiry; interruption/rollback; real-device update |
| M7 / v1.0.0 | Documented validated hardware, enclosure and release | End-to-end physical meter soak test; install from clean checkout; licence audit |

M0, M1 and M2 are completed. M2 physical display/replay acceptance was reported
by the user on 2026-10-23; electrical meter compatibility remains unverified.
M3 is accepted by user report on 2026-10-24 (docs/M3.md). M4 is accepted by
user report on 2026-10-26: stability, S1/access, live MQTT/recovery/discovery and
TLS acceptance points were confirmed. The revised M5 scope was accepted by
user report on 2026-10-26 and is complete (local v0.5.0). M6 web firmware update is accepted by user report on 2026-10-26 (local v0.6.0);
former M6 hardware/release moves to M7 by user request.
See M4.md and M5.md for evidence limits; electrical measurements remain open.
Local Git only; no public release. See CHANGELOG.md for checks and limitations.

User-requested exception (2026-10-25): bounded runtime simulation enable/sequence
settings with save-and-restart are implemented before full M5, replacing the
compile-time replay controls. This does not complete M5 or waive the then-pending
M4 physical/broker acceptance, subsequently accepted on 2026-10-26. No additional milestone tag is created.

M5 scope revised by user on 2026-10-26: omit export/import, replay pause/speed/
loop-toggle and capture copy/download. Item 6 (read-only diagnostics page) is
implemented and accepted by the user on 2026-10-26. Removed items are not
requirements for closure.

M6 added by user on 2026-10-26: firmware upload through Full-access Settings,
a script producing an application-only .bin and date-based firmware versions
(YYYY.MM.DD). Implemented and accepted by user report on 2026-10-26; see M6.md for
USB-layout migration, the acceptance checklist and evidence limitations.
