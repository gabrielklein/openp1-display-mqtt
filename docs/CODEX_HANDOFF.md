# M6 web update; M7 hardware/release

Read AGENTS.md, docs/REQUIREMENTS.md, HARDWARE.md, ARCHITECTURE.md, ROADMAP.md
and M5.md first. M4 and revised M5 were accepted by the user on 2026-10-26,
with local tags v0.4.0 and v0.5.0. No independent electrical measurements,
exact flashed revision or soak durations were supplied. No agent flashing or
public publishing is claimed. M6 now covers Full-only web firmware update, the .bin build script and date
versions; accepted by user report on 2026-10-26 with local v0.6.0 (M6.md).
Exact flashed hash and detailed fault-test procedures were not supplied. Former M6 hardware/electrical, prolonged real-meter validation,
clean install and licence/release audit move to M7.

The user removed export/import, pause/speed/loop-toggle and capture copy/download
from M5. Do not reintroduce them. Persisted UART settings and read-only raw/
decoded/error diagnostics are implemented and accepted. Do not claim v1.0.0
ready before M7 checks and user acceptance.

S1 defaults off until its GPIO35 pull-up is verified. Never flash or publish
without user instruction. Never power from USB and meter simultaneously.
Preserve UART/replay shared parsing, exact raw bytes, source MQTT isolation and
last-good readings. Keep acquisition off network waits, retain the low-RAM
band renderer and bound every upload/copy. Test malformed input and access expiry.

Use git --git-dir=.git-local --work-tree=. in this workspace; no remote is
configured. Original personal captures were removed. Tests use anonymized test/fixtures inputs;
do not restore original captures from historical commits.
