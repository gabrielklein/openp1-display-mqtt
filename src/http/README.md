# HTTP

server.cpp serves setup/read/raw/page routes; admin.cpp guards settings with
Full/origin/CSRF; stream.cpp owns bounded access-checked SSE; firmware.cpp streams
validated application uploads into an inactive OTA slot. See ../../docs/API.md
and ../../docs/FIRMWARE_UPDATE.md.
