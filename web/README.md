# Embedded web pages

`pages.hpp` contains the original offline dashboard and three-step Wi-Fi wizard:
network cards/manual SSID, credentials, connection progress and IP handoff.
HTML, CSS and JavaScript are compiled into firmware without npm/CDN dependencies.
No NetWizard code/assets are included. `node scripts/test-web.cjs` exercises
embedded scripts; `sh scripts/test-http.sh` exercises real HTTP handlers with
host transport doubles. Mobile/captive visual checks remain pending.

M4 `settings.hpp` adds Full-only access/MQTT settings with explicit secret
replacement/clearing; no credentials are fetched into the page. The dashboard
shows its settings link only during effective Full access.

The dashboard and settings use a consistent light palette, responsive cards,
larger touch controls and visible hover/pressed/keyboard-focus states. Settings
section links jump directly to each group. Every save shows pending/success/error
feedback beside its button and in the page status. Save controls are disabled
while a request runs; duplicate submissions are ignored. Default-sequence loading
also reports that a save/restart is still needed. Feedback uses live regions and
motion respects the browser's reduced-motion preference.

Script checks cover delayed saves, duplicate submissions and error recovery.
Visual desktop/mobile acceptance remains pending: the assistant environment
denied the local preview server's socket bind. No external assets are loaded.

Settings shows read-only device uptime (days/hours/minutes/seconds) from the
guarded status API, refreshed every 10 seconds with free RAM. Failed reads mark
uptime unavailable rather than advancing a browser-only counter. Settings API
requests share a serial queue: the previous five simultaneous startup requests
could exceed the server's three client sockets and trigger LRU disconnection.
Both readings and settings requests time out after eight seconds, including JSON
body consumption. The queue continues after failure. PUT requests are never
automatically retried; a timeout reports an unknown save result. MQTT status polling
recovers after transient errors, stopping on access denial. All access guards
remain enforced on the server. Device stability still needs physical validation.

The status endpoint copies only counters/flags/IP, without allocating the full
measurement snapshot. This keeps uptime available when a large contiguous
snapshot allocation would fail. Settings displays HTTP status on API errors;
physical intermittent failures still require device evidence to diagnose fully.

MQTT debug capture/history and its controls/routes have been removed. Settings
shows connection state, errors and accepted-publication count via the small
Full-only `/api/v1/mqtt/status` API every five seconds. The dashboard displays
successfully decoded meter notifications since restart. Counts include simulation;
MQTT counts include all locally published topics, not broker-generated LWT.

The MQTT JSON `/raw` option, chunk publisher and queue are removed. `/raw`
and `/hdlc` use the independent raw/HDLC hex switch; measurement topics
use the text measurement switch. HTTP raw diagnostics are
unaffected. The MQTT configuration API no longer exposes or accepts `raw`.

Compatibility topics no longer contain `/text/`. A separate default-off
"Publish individual OBIS values" checkbox emits `<prefix>/<mode>/1-0:51.7.0`
style topics with scaled numeric/plain-text payloads. This is independent of
the compatibility-summary and hex switches. Old saved MQTT settings migrate
without enabling OBIS publication or losing credentials.

The "Publish JSON state topic (/state)" switch independently controls MQTT state
publication, default on. Home Assistant discovery requires state output; existing
entities stop receiving updates while disabled. HTTP readings and other MQTT
output switches remain independent. Old saved profiles migrate with state on.

`access.hpp` is the English locked-browser shell with S1 instructions. It waits
on an empty 204/403-only Full-access probe and automatically opens `/` after a
physical grant. Protected APIs remain guarded; the probe refuses setup AP use.

The dashboard begins with a net-grid-power gauge, then Wi-Fi signal quality, then
connection/source. The Display settings set its symmetric kW limit (default ±5).
Export/Import watts appear beneath; the marker clamps without hiding an overrange
number. Absent export uses an explicitly labelled zero at the user's request;
present-but-stale/invalid export keeps the gauge unavailable. Missing import is
never zero-filled. Power and signal reuse the existing readings poll.

M5 adds Meter UART controls (baud/parity/stop bits/inversion) behind Full access
and CSRF, with bounded strict server validation. Defaults come from the board
profile until settings are saved. Save displays the existing restart page with
UART-specific text; boot settings apply only on restart, without changing source.
The user removed diagnostic export/import and advanced replay controls from M5.

`diagnostics.hpp` is the View/Full read-only raw/decoded/error page. It uses two
sequential, timed reads per refresh; access loss clears all data and opens the S1
waiting shell. Text is inserted via textContent, including unknown meter strings.
It exposes the existing bounded raw tail, scalar/string values and latest error,
not full frame history or nested A-XDR trees. No export/import/copy/download UI.

M6 adds Full-only firmware GET/PUT and the Firmware update panel. Browser PUT
sends the .bin as application/octet-stream with session CSRF, stops polling while
uploading, and shows the restart page only after validation/boot-selection success.
The installed date version comes from the ESP-IDF descriptor. Unsupported old
layouts need one USB installation; firmware files are application-only images
from scripts/build-firmware.py. See M6.md for bounds, rollback and pending tests.
