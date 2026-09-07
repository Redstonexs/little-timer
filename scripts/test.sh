#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p build
"${CXX:-g++}" -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc \
    tests/timer_tests.cpp src/timer.cpp src/sound.cpp -o build/timer_tests
./build/timer_tests
