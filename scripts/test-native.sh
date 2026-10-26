#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc/meter src/meter/hex.cpp test/native/test_hex.cpp -o "$test_dir/hex"
"$test_dir/hex"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp src/input/pipeline.cpp test/native/test_input.cpp -o "$test_dir/input"
"$test_dir/input"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp src/input/pipeline.cpp src/display/view.cpp test/native/test_protocol.cpp -o "$test_dir/protocol"
"$test_dir/protocol"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc test/native/test_network.cpp -o "$test_dir/network"
"$test_dir/network"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc src/meter/axdr.cpp test/native/test_presentation.cpp -o "$test_dir/presentation"
"$test_dir/presentation"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc test/native/test_access.cpp -o "$test_dir/access"
"$test_dir/access"

c++ -std=c++17 -fno-exceptions -Wall -Wextra -Werror -pedantic -Isrc test/native/test_memory.cpp -o "$test_dir/memory"
"$test_dir/memory"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp src/input/pipeline.cpp src/input/simulation.cpp test/native/test_simulation.cpp -o "$test_dir/simulation"
"$test_dir/simulation"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp src/input/pipeline.cpp test/native/test_mqtt_text.cpp -o "$test_dir/mqtt-text"
"$test_dir/mqtt-text"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc src/meter/hex.cpp src/meter/hdlc.cpp src/meter/axdr.cpp src/meter/processor.cpp src/input/pipeline.cpp test/native/test_allocation_recovery.cpp -o "$test_dir/allocation"
"$test_dir/allocation"
echo 'Native allocation recovery, MQTT text, simulation, memory, access, presentation, network, hex, input, protocol and display-model tests passed'
