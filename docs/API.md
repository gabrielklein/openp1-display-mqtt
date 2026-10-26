# Current HTTP API

JSON APIs are under `/api/v1`. Errors normally use
`{"error":{"code":"…","message":"…"}}`. None blocks ordinary API reads;
View permits readings; Full permits settings and firmware updates. Checks apply
to every request and ongoing SSE/upload operation. Setup AP cannot access these
station APIs. No wildcard CORS or application password is provided.

| Method | Path | Minimum access | Purpose |
| --- | --- | --- | --- |
| GET | `/api/v1/status` | View | Version, uptime, source, connectivity, counters and heap |
| GET | `/api/v1/values` | View | Bounded decoded snapshot and status |
| GET | `/api/v1/raw` | View | Bounded raw chunks and counters |
| GET | `/api/v1/events` | View | SSE; closes on loss of access |
| GET | `/api/v1/session` | Full | Session CSRF token |
| GET | `/api/v1/access` | View | Persisted policy and temporary-grant state |
| PUT | `/api/v1/access` | Full | Persist access policy and restart |
| GET, PUT | `/api/v1/serial` | Full | UART settings |
| GET, PUT | `/api/v1/simulation` | Full | Enable flag, layer and bounded sequence |
| GET, PUT | `/api/v1/display` | Full | Rotation and symmetric web gauge limit |
| GET, PUT | `/api/v1/mqtt` | Full | Broker/output settings; secrets omitted from GET |
| GET | `/api/v1/mqtt/status` | Full | Connection, accepted-publication count and last error |
| GET, PUT | `/api/v1/firmware` | Full | OTA capability/version and binary upload |

Settings PUTs require `X-CSRF-Token`, canonical station Host and same-origin
Origin when supplied. JSON settings have strictly validated types/keys and
bounded bodies. Firmware PUT instead uses `application/octet-stream`, at most
3 MiB and a 180-second deadline. See [firmware update](FIRMWARE_UPDATE.md).
Serial, simulation and policy saves schedule restart after successful persistence.

Values expose canonical `obis`, readable `obis_display`, numeric or string
`value`, timestamps and stale status. Numeric values include raw/scaler/unit
information. Text/octet encodings are UTF-8 or lossless hex; identifiers remain
strings so leading zeroes are not lost. Snapshots are bounded and independently
sampled, not a historical capture. MQTT secrets are represented by configured
flags rather than fetched credentials/certificate contents.

## Browser and setup routes

`/` is the dashboard, `/diagnostics` is read-only View/Full, and `/settings` is
Full-only. Denied browser navigation shows the static `/unlock` S2 waiting page.
`/access-check` returns an empty 204 only when Full is available, otherwise 403;
it reveals no configuration/readings/token and is always refused during setup.

Provisioning provides setup-only `/setup`, `/setup/token`, `/setup/scan`,
`/setup/status` and POST `/setup/wifi`. It exposes only Wi-Fi setup data, scan
results and connection progress/IP handoff. Captive portal detection is best effort.

There is no generic `/config`, WebSocket `/live`, simulator file-upload/control,
MQTT debug API or diagnostic export API. Simulation uses its settings endpoint;
firmware binary upload is a separate feature.
