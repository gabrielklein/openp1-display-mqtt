# Embedded web pages

Offline HTML/CSS/JS is compiled into the firmware without npm/CDN dependencies.
pages.hpp provides setup and dashboard; settings.hpp provides Full-only settings
and OTA; diagnostics.hpp is read-only View/Full; access.hpp waits for S2 Full access.

Settings serializes timed API calls, shows action feedback and never retries
mutations automatically. OTA pauses polling and shows the restart page after
success. Access is enforced on the server, including active streams/uploads.

Run node scripts/test-web.cjs and sh scripts/test-http.sh from the project root.
See ../docs/API.md, ../docs/MQTT.md and ../docs/FIRMWARE_UPDATE.md.
