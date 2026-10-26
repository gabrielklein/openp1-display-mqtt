# Development handoff

Read AGENTS.md, REQUIREMENTS.md, ../hardware/HARDWARE.md, ARCHITECTURE.md and ROADMAP.md.
The current code includes accepted M0–M6 functionality. Continue with the M7
hardware/release checks in TEST_PLAN.md; do not describe the final release or
unmeasured electrical compatibility as validated.

Git was reset to an anonymized initial snapshot. Old milestone commits/tags and
the history bundle are gone. Use `git --git-dir=.git-local --work-tree=.` in this
workspace; no remote is configured. `docs/pre-release/` is historical material,
not current instructions or available Git history. Do not restore personal captures.

Preserve portable shared UART/replay parsing, exact live raw bytes, last-good
readings, bounded memory and the eight-row DMA renderer. Fixtures use placeholder
meter identifiers 12345678/1234567; live diagnostics may expose actual identifiers.
Do not add credentials or personal configuration to examples, logs or Git.

S1 defaults off until the GPIO35 pull-up is verified. Preserve server-side access,
CSRF/origin checks, ongoing upload/stream checks and verified MQTT TLS. There is
no application login. Do not reintroduce the removed debug console, diagnostic
export/import/copy/download or advanced replay controls.

Never flash or publish without user instruction. Never power from meter and USB
simultaneously. Record meaningful checks/limitations and commit completed work.
Current build/test commands are in README.md; do not tag a milestone until its
checks and user acceptance pass.
