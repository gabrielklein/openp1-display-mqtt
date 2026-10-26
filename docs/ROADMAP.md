# Current status and remaining work

The current snapshot includes completed software through M6, accepted by user
report. These labels describe functional scope; they are not existing Git tags.
Git was reset to one anonymized initial snapshot and new commits build on it.
There is no remote or public release.

| Scope | Status |
| --- | --- |
| M0–M2: platform, shared UART/replay parser and LCD | Complete; display/replay accepted |
| M3: Wi-Fi provisioning and dashboard | Accepted |
| M4: policies/S2, MQTT, TLS and Home Assistant Discovery | Accepted |
| M5: persistent UART settings and read-only diagnostics | Accepted, reduced scope |
| M6: Full-only web update, date versions and packaging script | Accepted |
| M7: hardware/electrical, prolonged real-meter test and release | Open |

M6 acceptance covers the proposed migration/update/version/settings/MQTT,
invalid-image and rollback checklist. No exact flashed hash, detailed fault-test
procedure or timing evidence was supplied. User acceptance is not an independent
electrical measurement or exhaustive fault-injection result.

## M7 completion criteria

Follow [TEST_PLAN.md](TEST_PLAN.md): verify hardware and supply compatibility,
real-meter reading consistency, 48-hour stability and repeated Wi-Fi/broker
recovery. Confirm access/configuration and topic separation on the actual device.
Complete clean-install/build/package checks, licence/privacy review and enclosure
measurements before release. Record version/hash, duration and observed results.

Capture export/import/copy/download, advanced replay controls and the MQTT debug
console are outside the agreed scope. Do not reintroduce them as unfinished work.
Historical development notes are isolated in pre-release/ and are not current
requirements, release instructions or references to available commits/tags.
