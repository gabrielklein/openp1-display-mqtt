# M7 hardware and release test plan

M7 was closed by explicit user acceptance on 2026-10-29; see
[M7 acceptance](M7_ACCEPTANCE.md) for results and evidence limits. This checklist
remains the reference for retesting; closure does not assert unavailable measurements.

Record each result with date, firmware date version, application SHA256, test,
duration, pass/fail and observations. Completed M6 acceptance is documented in
FIRMWARE_UPDATE.md; do not treat it as proof of electrical compatibility or soak.

## 1. Hardware

Confirm actual 16 MiB flash, pinout, RX electrical compatibility, GPIO35 pull-up
and stable meter supply during Wi-Fi activity. Record measurements and actual
meter/board identification privately where appropriate. Check the enclosure fit,
strain relief and antenna clearance; complete BOM/dimensions before release.

**Disconnect the complete meter cable before USB. Never power from both sources.**
Electrical checks must follow the conditional hardware guidance in [HARDWARE.md](../hardware/HARDWARE.md);
no mains-side intervention is part of this plan.

Pass: verified compatible signal/supply, stable operation and no abnormal heating
or unexpected reset. Do not infer measurements from successful decoding alone.

## 2. Real acquisition — at least 30 minutes

Disable simulation. Compare power, phase voltages and energy indexes against the
meter; exercise import/export when available. Check increasing decoded-message
count and reasonable parser errors. Verify unavailable/stale readings keep their
last-good value or show unavailable, without fabricated zeroes.

Pass: consistent values, no repeated unexplained decode errors or unexpected reset.

## 3. Continuous operation — at least 48 hours

Use the real meter, intended MQTT outputs and regular web access. Record initial,
periodic and final uptime, received/sent counts, parser/MQTT errors, free heap,
largest free block and minimum heap when available.

Pass: no unexpected restart, web remains responsive and no continuing heap decline
under a comparable workload. A single memory reading is not a leak assessment.

## 4. Recovery — repeat each three times

| Action | Expected outcome |
| --- | --- |
| Wi-Fi unavailable for five minutes | LCD/acquisition continue; automatic reconnection |
| Broker unavailable for five minutes | Error visible; automatic MQTT recovery |
| Restart access point | Network/MQTT recover without device restart |
| Normal device restart | Settings preserved; acquisition and MQTT resume |

Retain useful logs privately; anonymize identifiers and network details before sharing.

## 5. Access and persistence

Verify None/View/Full against the actual API/UI, including firmware update denial
without Full. MQTT must continue in None. If S2 is enabled, test short grant,
300-second expiry and return to persisted policy. Check ongoing SSE/upload access
loss. Confirm UART, MQTT, display, source and access settings survive restart.

Pass: no unauthorized mutation/read, no secrets in GET/log output, expected saves
and restart feedback. Perform Wi-Fi reset only when ready to provision again.

## 6. MQTT compatibility and source isolation

For example prefix `/sensors/power/p1meter`, verify real `<prefix>/active` and
simulation `<prefix>/simulation/active`. Check the preserved leading slash, each
independent publication switch, payload order/units and Discovery if used. Confirm
simulation never mixes into the real branch. Restore intended UART settings after.

## 7. Clean build and release

Build from a clean checkout using pinned dependencies. Run all README checks and
package the date-versioned binary; verify checksum and displayed version. Validate
installation/recovery instructions, licence, no personal data/secrets in tracked
files or release artifacts, and document hardware/test limitations.

Close M7 only after results and user acceptance are recorded. No public publishing,
hardware flash or release tag is authorized merely by this test plan.
