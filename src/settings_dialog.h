#pragma once

#include "platform.h"
#include "timer.h"
#include "win_audio.h"

namespace little_timer {

struct SettingsDialog {
    Settings value;
    bool remember = false;
    float scale = 1;
    HFONT font = nullptr;
    HBRUSH background = nullptr;
    HBRUSH field = nullptr;
    AudioPlayer audio;
    std::wstring message;

    bool show(HWND owner, HINSTANCE instance);
    bool read(HWND dialog);
    static INT_PTR CALLBACK procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
};

} // namespace little_timer
