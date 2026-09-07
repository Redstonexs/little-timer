#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

# Override MINGW_BIN to use a locally extracted toolchain without installing it.
tool_prefix="${MINGW_BIN:-}"
compiler="${CXX_MINGW:-${tool_prefix}i686-w64-mingw32-g++}"
resource_compiler="${WINDRES:-${tool_prefix}i686-w64-mingw32-windres}"
mkdir -p build dist

flags=(-std=c++17 -Os -Wall -Wextra -Wpedantic -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DWINVER=0x0601
       -Isrc -ffunction-sections -fdata-sections)
link_flags=(-mwindows -municode -static -static-libgcc -static-libstdc++
            -Wl,--gc-sections,--strip-all,--no-insert-timestamp,--dynamicbase,--nxcompat
            -Wl,--major-os-version,6,--minor-os-version,1,--major-subsystem-version,6,--minor-subsystem-version,1)
sources=(src/main.cpp src/app.cpp src/ui.cpp src/settings_dialog.cpp src/win_audio.cpp src/timer.cpp src/sound.cpp)

"$resource_compiler" -Iresources resources/app.rc -O coff -o build/resources.o
"$compiler" "${flags[@]}" "${sources[@]}" build/resources.o "${link_flags[@]}" \
    -lgdiplus -lwinmm -lcomctl32 -lgdi32 -luser32 -lshell32 -o dist/LittleTimer.exe
if [[ "${1:-}" == "--qa" ]]; then
    "$compiler" "${flags[@]}" -DLITTLE_TIMER_DIAGNOSTICS "${sources[@]}" tests/windows_diagnostics.cpp \
        build/resources.o "${link_flags[@]}" -lgdiplus -lwinmm -lcomctl32 -lgdi32 -luser32 -lshell32 -o build/LittleTimerQA.exe
    "$compiler" "${flags[@]}" tests/timer_tests.cpp src/timer.cpp src/sound.cpp \
        -static -static-libgcc -static-libstdc++ -o build/timer_tests.exe
fi
printf 'Built dist/LittleTimer.exe\n'
