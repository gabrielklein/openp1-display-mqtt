#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
idf_path="${IDF_PATH:-$HOME/.platformio/packages/framework-espidf}"
cjson_dir="$idf_path/components/json/cJSON"
if [ ! -f "$cjson_dir/cJSON.c" ]; then
    echo 'Set IDF_PATH to the pinned ESP-IDF 5.3.1 package to run HTTP handler tests.' >&2
    exit 1
fi
san_flags=""
if [ "${OPENP1_SANITIZE:-0}" = 1 ]; then
    san_flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
fi
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
cc $san_flags -std=c99 -I"$cjson_dir" -c "$cjson_dir/cJSON.c" -o "$test_dir/json.o"
c++ $san_flags -std=c++17 -Wall -Wextra -Werror -pedantic -Itest/native/http_fakes -Isrc -I"$cjson_dir" \
    src/firmware/update.cpp src/http/firmware.cpp src/config/serial.cpp src/http/server.cpp src/http/admin.cpp src/http/stream.cpp src/simulation/service.cpp src/input/simulation.cpp src/input/pipeline.cpp src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp \
    test/native/test_http.cpp "$test_dir/json.o" -Wl,--wrap=malloc -o "$test_dir/http"
"$test_dir/http"
c++ $san_flags -std=c++17 -Wall -Wextra -Werror -pedantic -Itest/native/http_fakes -Isrc -I"$cjson_dir" \
    src/meter/axdr.cpp test/native/test_mqtt.cpp "$test_dir/json.o" -o "$test_dir/mqtt"
"$test_dir/mqtt"
c++ $san_flags -std=c++17 -Wall -Wextra -Werror -pedantic -Itest/native/http_fakes -Isrc -I"$cjson_dir" \
    src/mqtt/service.cpp src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp src/input/pipeline.cpp test/native/test_mqtt_service.cpp "$test_dir/json.o" \
    -Wl,--wrap=time -o "$test_dir/mqtt-service"
"$test_dir/mqtt-service"
"$test_dir/mqtt-service" simulation
"$test_dir/mqtt-service" simulation v3
"$test_dir/mqtt-service" simulation v4 padding
c++ $san_flags -std=c++17 -Wall -Wextra -Werror -pedantic -Itest/native/http_fakes -Isrc \
    src/simulation/service.cpp src/input/simulation.cpp src/input/pipeline.cpp src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp test/native/test_simulation_service.cpp -o "$test_dir/simulation-service"
"$test_dir/simulation-service"
"$test_dir/simulation-service" saved
"$test_dir/simulation-service" corrupt
c++ $san_flags -std=c++17 -Wall -Wextra -Werror -pedantic -Itest/native/http_fakes -Isrc -I"$cjson_dir" \
    src/meter/axdr.cpp test/native/test_payload_allocation.cpp "$test_dir/json.o" -o "$test_dir/payload-allocation"
"$test_dir/payload-allocation"
c++ $san_flags -std=c++17 -Wall -Wextra -Werror -pedantic -Itest/native/http_fakes -Isrc \
    src/config/serial.cpp src/input/pipeline.cpp src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp test/native/test_serial_service.cpp -o "$test_dir/serial-service"
"$test_dir/serial-service"
c++ $san_flags -std=c++17 -Wall -Wextra -Werror -pedantic -Itest/native/http_fakes -Isrc \
    src/firmware/update.cpp test/native/test_firmware_startup.cpp -o "$test_dir/firmware-startup"
"$test_dir/firmware-startup"
"$test_dir/firmware-startup" unhealthy
echo 'HTTP/SSE and MQTT configuration, payload and service tests passed (host doubles)'
