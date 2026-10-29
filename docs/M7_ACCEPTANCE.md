# M7 acceptance — 2026-10-29

## Decision and evidence

M7 is closed by explicit user acceptance. After reviewing the remaining M7 scope,
the project owner confirmed: “Je confirme que tout marche, on peut fermer la
release M7”. This records a successful user-reported outcome for the milestone.
No hardware was flashed and no public release was published by the agent.

The installed firmware hash, exact test duration, electrical measurements,
hardware identification and detailed fault/recovery logs were not supplied.
Do not infer independently measured electrical compatibility or a documented
48-hour run from this acceptance. TEST_PLAN.md remains the reproducible checklist
for future installations and retesting. All hardware power/interface precautions
remain applicable; S2 stays disabled by default until its pull-up is verified.

## Local closure checks

On 2026-10-29:

- `sh scripts/test-native.sh`: passed.
- `node scripts/test-web.cjs`: passed.
- `sh scripts/test-http.sh`: passed (HTTP/SSE and MQTT host doubles).
- Firmware build/package attempted for VERSION `2026.10.26`: not completed.
  The default PlatformIO core directory was not writable in the sandbox.
  A temporary core using the installed pinned platform and tools reached CMake,
  then failed with “Multiple ways to build the same target” for
  `esp_efuse_fields.c.o`. An earlier attempt also could not download Python
  dependencies because network name resolution was unavailable.

No new binary, clean-install result, checksum validation or release tag is
claimed. The firmware VERSION is unchanged; this is a milestone acceptance
record, not a newly validated firmware build. A reproducible clean build/package
and artifact audit are still required before tagging or distributing a new
release artifact. Existing uncommitted distribution-script work is excluded
from the milestone closure commit.
