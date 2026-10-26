# Country and meter profiles

These pages collect installation-specific UART settings and meter/interface
information. Choose the country, provider and exact meter model before applying
a profile in **Full Settings → Meter UART**. Saving UART settings restarts the
device; disable simulation separately to acquire from the physical meter.

| Country | Provider / meter | Profile |
| --- | --- | --- |
| Switzerland (Suisse) | Romande Energie / ISKRAEMECO AM550 | [Settings and interface](switzerland/romande-energie.md) |

Profiles are documentation, not selectable firmware presets. A provider name
alone does not establish compatible settings: model, firmware, enabled port and
local installation can differ. Each page states where its information came from.

## Contributing a profile

Add a page under `<country>/<provider-or-meter>.md` and list it above. Include:

- Country/provider, manufacturer, model and exact interface/protocol.
- Baud rate, data bits, parity, stop bits and RX inversion.
- Activation requirements, update interval and payload size if known.
- Source of the settings, observed results and any unverified details.

Do not include meter serial numbers, customer details, credentials or private
network addresses. Reference the shared [hardware guidance](../HARDWARE.md)
rather than assuming that all P1 connectors have the same electrical interface.
