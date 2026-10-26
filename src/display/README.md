# Display

view.hpp/cpp provides portable view/raster rendering. st7789.cpp owns SPI and the
latest-view mailbox, rendering color bands with a 3840-byte DMA buffer. The LCD
shows net power, phase voltages, Wi-Fi/MQTT/RX and access/IP status, with optional
180° rotation. See ../../docs/HARDWARE.md and ../../docs/ARCHITECTURE.md.
