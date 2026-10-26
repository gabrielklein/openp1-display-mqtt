# MQTT configuration and outputs

Full Settings configures one shared broker connection and publication prefix.
Plain MQTT defaults to port 1883. Optional TLS verifies certificate chain and
hostname, using the public trust bundle or a supplied private CA; time must be
valid. Credentials/CA are replaced or explicitly cleared, never returned by GET.

The default prefix is `openp1/<stable-device-id>`. A leading `/` is accepted and
preserved; a trailing `/`, wildcard `+`/`#` or whitespace is rejected. MQTT topics
with and without a leading `/` are distinct.

For prefix `/sensors/power/p1meter`:

- Real: `/sensors/power/p1meter/active`.
- Simulation: `/sensors/power/p1meter/simulation/active`.

There is no `/normal` or `/text` component. All outputs, LWT and discovery use
the selected root; MQTT continues even when web access is None.

## Independent output switches

| Switch | Default | Outputs |
| --- | --- | --- |
| JSON state | On | `state`: values, timestamps, stale flags and source |
| Text measurements | On | `id`, `active`, `voltage`, `current`, `metering` |
| Raw/HDLC hex | On | `raw`, `hdlc` |
| Individual OBIS | Off | e.g. `1-0:51.7.0` with a scaled/plain-text value |
| Home Assistant Discovery | Off | Retained entity configuration; requires JSON state |

MQTT itself defaults disabled. Availability/LWT is retained; streaming state,
summary, OBIS and hex messages are unretained QoS 0. Disabling state suppresses
new discovery but does not delete existing retained entities. Simulation has
separate Home Assistant identities. There is no JSON `/raw` publisher.

| Topic suffix | Payload |
| --- | --- |
| `id` | Configured label, default ISK; blank omits it, not manufacturer detection |
| `active` | Import W, export W, cumulative import kWh, cumulative export kWh |
| `voltage` | Mean phase voltage, L1, L2, L3 in V |
| `current` | Sum of phase currents, L1, L2, L3 in A |
| `metering` | Import tariff 1/2, export tariff 1/2 in Wh |
| `raw` | Original complete HDLC frame hex, including flags/checksums |
| `hdlc` | Reassembled LLC/APDU hex |

Summary groups require fresh compatible values with matching timestamps; missing
or stale groups are omitted. Derived voltage/current summaries are compatibility
outputs, not additional transmitted OBIS objects. OBIS values apply transmitted
scalers and omit missing/stale/nonfinite/unsupported values. Direct LLC/APDU
simulation emits no fabricated raw frame.

Settings reports connection state, last error and boot-lifetime messages accepted
by the MQTT client, not delivery acknowledgements from a subscriber. It stores
no debug console/payload history. The dashboard's received count instead tracks
successfully decoded meter notifications, including simulation.
