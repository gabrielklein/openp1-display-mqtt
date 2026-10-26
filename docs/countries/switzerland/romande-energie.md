# Switzerland / Romande Energie

## Meter and interface

| Parameter | Profile |
| --- | --- |
| Country | Switzerland (Suisse) |
| Electricity provider | Romande Energie |
| Meter manufacturer | ISKRAEMECO |
| Meter type | AM550 |
| Customer interface | P1 DSMR V5.0 |
| Protocol | DLMS/COSEM |
| Reported payload length | 154 bytes |
| Transmission mode | Push: unsolicited meter data |
| Reported transmission interval | Every 5 seconds while ACTIVATE is connected to +5 V |

Source: installation details supplied by romande énergie. The reported payload length
is not a fixed parser limit: HDLC frames and segmented notifications must still
be handled according to their declared lengths and checksums. Do not truncate
incoming data to 154 bytes.

## UART settings

Open **Settings → Meter UART** with Full access and enter:

| Setting | Value |
| --- | --- |
| Baud rate | **115200** |
| Data bits | **8** (fixed by the current firmware) |
| Parity | **None** |
| Stop bits | **1** |
| Invert RX signal | **Enabled** |

Choose **Save UART and restart**. For live acquisition, ensure simulation is
disabled in **Settings → Simulation**, saving/restarting if changed. These values
match the current initial UART defaults; existing saved settings take precedence.

## Activation and wiring

The supplied profile reports push data every five seconds when the interface's
**ACTIVATE** input is connected to **+5 V**. This activation connection is separate
from the ESP32 UART RX input. Identify the actual activation/power contacts using
the meter's interface documentation before wiring; connector appearance alone is
not sufficient. This page does not independently verify those electrical details.

Use the conditional [hardware/pinout guidance](../../../hardware/HARDWARE.md): meter data is
routed to GPIO27, with a proposed 10 kΩ pull-up to 3.3 V for a compatible
open-collector interface. Never connect a push-pull 5 V signal directly to RX.
Disconnect the complete meter cable before USB; never power from both sources.

## Check operation

1. After restart, confirm the source is the physical meter rather than Simulation.
2. Check that successfully decoded messages arrive at approximately five-second
   intervals while the reported activation condition is satisfied.
3. Compare displayed power, phase voltages and energy indexes with the meter.
4. If decoding fails, inspect Diagnostics for raw bytes and the latest parser
   error; check UART settings, interface activation and complete frame reception.

Live diagnostics and raw MQTT output may contain actual meter identifiers.
Anonymize them before sharing. Broader electrical and long-run validation remains
part of the [M7 test plan](../../TEST_PLAN.md).
