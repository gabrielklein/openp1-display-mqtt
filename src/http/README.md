# HTTP

`server.cpp` serves setup/read/raw APIs; `admin.cpp` enforces Full/CSRF for
policy and MQTT settings; `stream.cpp` owns bounded, access-checked SSE.
Full configuration, upload/replay controls and capture export remain M5.
See docs/API.md and docs/M4.md.
