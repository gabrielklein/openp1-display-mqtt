# Access

`control.hpp` implements portable None/View/Full grants, 300-second expiry,
debounce and eight-second Wi-Fi-reset hold. GPIO integration is in the network
worker. S1 is disabled until its pull-up is verified and enabled in menuconfig.
See docs/M4.md for enforcement, tests and physical acceptance still pending.
