@echo off
setlocal
pushd "%~dp0.."
where i686-w64-mingw32-g++ >nul 2>nul
if errorlevel 1 (
    echo A 32-bit MinGW-w64 compiler using MSVCRT is required.
    echo Put i686-w64-mingw32-g++ and windres on PATH, then run this script again.
    popd
    exit /b 1
)
if not exist build mkdir build
if not exist dist mkdir dist
i686-w64-mingw32-windres -Iresources resources\app.rc -O coff -o build\resources.o
if errorlevel 1 goto failed
i686-w64-mingw32-g++ -std=c++17 -Os -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -Isrc -ffunction-sections -fdata-sections src\main.cpp src\app.cpp src\ui.cpp src\settings_dialog.cpp src\win_audio.cpp src\timer.cpp src\sound.cpp build\resources.o -mwindows -municode -static -static-libgcc -static-libstdc++ -Wl,--gc-sections,--strip-all,--no-insert-timestamp,--dynamicbase,--nxcompat -Wl,--major-os-version,6,--minor-os-version,1,--major-subsystem-version,6,--minor-subsystem-version,1 -lgdiplus -lwinmm -lcomctl32 -lgdi32 -luser32 -lshell32 -o dist\LittleTimer.exe
if errorlevel 1 goto failed
echo Built dist\LittleTimer.exe
popd
exit /b 0
:failed
popd
exit /b 1
