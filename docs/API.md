# API contract and implementation status

M4 implements dashboard/read/raw APIs, SSE `/api/v1/events`, access-policy and
MQTT settings plus isolated Wi-Fi setup. See [M4](M4.md) for endpoint schemas.
Complete config, diagnostics export and simulator routes below remain planned.
See [M3](M3.md) for response fields, bounds and acceptance status.

Base /api/v1. JSON responses; errors use {"error":{"code":"...","message":"..."}}. No credentials in GET responses.

| Method/path | Minimum access | Purpose |
| --- | --- | --- |
| GET /status | View | Firmware, source, connectivity, stale state |
| GET /values | View | Snapshot with OBIS, value, unit, timestamp |
| GET /raw | View | Bounded raw records with sequence/cursor |
| GET /diagnostics/export | View | Replayable text download |
| GET /config | Full | Settings; secret fields represented as configured booleans |
| PUT /config | Full | Validate and persist configuration |
| POST /simulator/upload | Full | Bounded .txt input |
| POST /simulator/control | Full | Start/pause/stop/speed/loop |
| WS /live | View | Snapshot updates and diagnostics |

Provisioning uses separate GET /setup and POST /setup/wifi routes, reachable only in Wi-Fi setup state. API None denies all ordinary API routes. Enabled S1 grants temporary Full for 300 seconds and then restores persisted policy. Check current access for each stream event and terminate streams when access is revoked.

Same-origin mutating requests with Origin/Host validation and CSRF protection. Do not enable wildcard CORS. Bound request sizes/rates. Configuration changes must not expose secrets in echoed errors. Full mode has no user authentication by explicit requirement.

M3 provisioning uses an original three-step ESP-IDF wizard. `GET /setup/scan`
returns asynchronous scan state/results, and `GET /setup/status` returns only
Wi-Fi provisioning state and the station IP after success. Both are setup-only;
ordinary read API remains forbidden until the setup AP closes. Detailed schemas
and timing are in [M3](M3.md#provisioning-endpoints). No NetWizard dependency.

Dashboard value entries include `obis_display` (`A-B:C.D.E`, `*F` retained when
F is not 255), while `obis` retains its canonical six dotted components.
`value` can be a JSON number or string. Text/octets include `value_encoding`
(`utf8` or lossless `hex`); `raw_value` is supplied only for numeric values.
Serial/GR are transmitted as strings, without numeric coercion or lost zeroes.

M4 adds GET/PUT `/api/v1/access`, Full-only GET `/settings`, `/api/v1/session`,
GET/PUT `/api/v1/mqtt`, View-only GET `/api/v1/raw` and SSE `/api/v1/events`.
GET MQTT never returns username/password/CA, only configured booleans. PUT uses
`X-CSRF-Token`, canonical station Host and same-origin Origin when supplied.
Existing SSE connections recheck policy on every chunk and close on loss of View.
Full config/exports/simulator APIs and WS `/live` are still unavailable.
