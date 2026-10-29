# Wi-Fi configuration from Settings — 2026-10-29

## Implemented behavior

Full Settings includes a nearby-network scan and manual SSID/password form.
Scanning reuses the bounded provisioning worker (16 results, ten-second refresh
limit). Scan results are inserted as browser text, never HTML.

The station-only `PUT /api/v1/wifi` validates an exact two-field object, limits
its body to 768 bytes, and checks canonical Host/origin, CSRF and Full access,
including a second Full check after receiving the body. Setup AP, View and None
cannot use these station routes. Saved credentials have no read endpoint and are
not included in responses or logs; the browser clears the password after submission.

Saving replaces the existing NVS Wi-Fi record and schedules the usual delayed
restart. Other settings and the active connection are preserved until restart.
Storage failure produces an error without scheduling a restart. The UI stops
polling on successful save and directs the user to the LCD IP on the new network.

The connection is not tested before saving. Incorrect credentials can make the
device unreachable; the existing reconnect behavior does not open a fallback AP.
Recovery is an enabled S2 Wi-Fi reset or USB servicing. The form explains this
before saving. Empty password explicitly selects an open network.

## Software validation

- `node scripts/test-web.cjs`: passed, including scan polling/selection, untrusted
  SSID text, UTF-8 byte limits, empty passwords, failure/403 feedback, password
  clearing, restart handoff and stopped status polling.
- `sh scripts/test-http.sh`: passed, including scan/access guards, exact input
  validation, size bounds, embedded NUL rejection, loss of Full during receive,
  Host/origin/CSRF rejection, scan conflicts, storage failure, preserved previous
  credentials on failed commit, and delayed restart after successful persistence.
- Pinned PlatformIO `espressif32@6.9.0` / ESP-IDF 5.3.1 application build and
  packaging passed for `2026.10.29`. The sandbox required a temporary writable
  PlatformIO core and a copy of the installed framework, with existing tools and
  Python dependencies reused. No platform/dependency versions were changed.
- Application and USB ZIP artifacts are generated under `dist/2026.10.29/`
  (ignored by Git). All four binary checksums, ZIP integrity, the embedded
  version and Wi-Fi UI were verified. The pre-existing distribution-script changes are not part
  of this feature commit.

This feature is subsequent to M7 acceptance. No flash, public publication,
new milestone tag or real-device acceptance is claimed.

## Device acceptance to perform

1. With Full access, scan and select the intended network; also try manual entry.
2. Save valid credentials, observe restart and reopen the LCD IP on the new network.
3. Confirm acquisition/MQTT resume and unrelated settings survive restart.
4. Confirm View/None cannot scan/change Wi-Fi; exercise temporary S2 Full expiry
   if the verified board profile enables S2.
5. Only with a recovery path available, verify invalid credentials show the
   existing reconnect state and the enabled S2 reset restores provisioning.

## Disconnected LCD hint — 2026-10-29

Configured devices with S2 enabled show `Hold S2 8s to reset` beside
`WiFi reconnecting` until connected. Setup guidance, active reset countdown and
the existing alternating temporary-access status retain their precedence.
S2-disabled builds keep the retry-delay text. Power and phase readings remain
visible; reset behavior and automatic reconnection are unchanged.

Validated the footer string/buffer and 240-pixel layout bounds, rebuilt the
pinned ESP-IDF firmware and package, checked all binary checksums and ZIP
integrity, and verified the hint is embedded in the packaged application.
The same-date `2026.10.29` artifacts were regenerated; their checksum identifies
the new image. Physical LCD/reconnect/button validation remains a device check.
No hardware was flashed.
