# Hardware

## Proposed ESP32 connections

| Function | GPIO / terminal |
| --- | --- |
| Meter data to UART RX | GPIO27 |
| Data pull-up | 10 kohm from GPIO27 to 3V3 |
| Meter grounds | GND |
| Meter power | Board 5V terminal |
| Logical S1 | GPIO35, right button on supplied pinout |
| Left/boot button | GPIO0 |
| ST7789V MOSI / SCLK | GPIO19 / GPIO18 |
| LCD CS / DC / reset / backlight | GPIO5 / GPIO16 / GPIO23 / GPIO4 |

GPIO35 has no internal pull-up. Verify the onboard resistor before relying on button reads. USB UART0 remains available for flashing/debug; use a separate UART controller for meter input. Do not use GPIO16 for RX.

## P1 reference, conditional on compatible meter

| P1 RJ12 pin | Signal | Proposed connection |
| --- | --- | --- |
| 1 | +5 V output | Board 5V |
| 2 | Data Request input | P1 +5 V |
| 3 | Data ground | Board GND |
| 4 | Not connected | Leave open |
| 5 | Data output, open collector | GPIO27, 10 kohm pull-up to 3V3 |
| 6 | Power ground | Board GND |

User-reported order is GND, RX, unused, GND, 5V, 5V: consistent with pins 6 to 1. This is a conditional correspondence, not verification of connector orientation. Cable colours are not a reliable standard. Identify numbered contacts or measure continuity with cable disconnected.

A pull-up does not make a push-pull 5 V output safe for ESP32. Verify the meter uses a compatible open-collector output. No mains-side work is involved. Power only from meter OR USB; disconnect the complete meter cable before USB programming.

Reference DSMR 5.0.2 pinout (electrical reference only; does not establish that this Swiss HDLC meter uses DSMR ASCII telegrams):
https://www.landisgyr.de/webfoo/wp-content/uploads/2025/03/P1-Companion-Standard.pdf

Display reference:
https://github.com/Xinyuan-LilyGO/TTGO-T-Display

RX inversion reference:
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/uart.html

## Bench checklist

2026-10-23 user observation: the physical display shows `OPENP1 UART`,
`NO READINGS ERR 0` and `NONE`, in black and white. This matches the current
monochrome renderer and UART startup view. It confirms visible text on the
screen. The user also confirms correct text orientation and no edge clipping.
Replay pagination, SIMULATION and stale
rendering were subsequently reported successful by the user in the requested
replay test. M2 display acceptance is complete on that basis; the exact flashed
revision was not supplied. Electrical compatibility remains unverified.

Confirm actual flash capacity, display colours/offset/orientation, GPIO35 polarity, RX idle voltage, serial baud/parity and meter supply under radio load. Record measured results in this file. Current status: basic screen and replay checked by user; electrical checks remain pending.

## M2 proposed software panel profile

ESP-IDF ST7789 driver: SPI2, 20 MHz, 240x135 landscape, XY swap and X
mirror, gap 40/53, inversion enabled; GPIOs remain as listed above. This
is an implementation based on the LILYGO reference, not a measured Tenstar
validation. Host raster rendering was inspected; physical display acceptance
was reported successful by the user on 2026-10-23. See [M2](M2.md) for the bench checklist. No flashing was done.

## M4 S1 software profile

S1 uses active-low GPIO35, internal pull resistors disabled, 50 ms debounce,
a short release for 300-second Full access and an eight-second Wi-Fi-only reset
hold with visible countdown. It is off by default in menuconfig until the
external/onboard pull-up is verified. No S1 bench test is claimed.
