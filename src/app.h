#pragma once

#include "platform.h"
#include "timer.h"
#include "win_audio.h"

#include <string>
#include <vector>

namespace little_timer {

enum Command { StartPause = 100, Reset, OpenSettings, Fullscreen, Audience, Mute };
struct App;

struct Window {
    App* app = nullptr;
    HWND hwnd = nullptr;
    bool audience = false;
    bool fullscreen = false;
    WINDOWPLACEMENT placement = {};
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
};

struct App {
    HINSTANCE instance = nullptr;
    Timer timer;
    AudioPlayer audio;
    Window control;
    Window stage;
    bool muted = false;
    bool remember = false;
    bool settingsOpen = false;
    bool keepAwake = false;
    bool diagnostic = false;
    float scale = 1;
    std::wstring configPath;
    std::wstring notice;
    std::wstring stageNotice;
    std::uint64_t stageNoticeUntil = 0;
    std::int64_t lastPaintSecond = -1;

    explicit App(HINSTANCE module);
    ~App();
    bool create(int show);
    void layout(Window& window);
    void paint(Window& window, HDC dc, int width, int height);
    void refresh();
    void tick(std::uint64_t now);
    void cue(Cue cue);
    void updateAwake();
    void command(int id, Window& source);
    void openSettings();
    void openAudience();
    void toggleFullscreen(Window& window);
    void load();
    void save();
    bool confirmReset();
    bool handleKey(MSG& message);
    static LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

bool writeUtf8(const std::wstring& path, const std::wstring& value);
std::wstring executablePath();

#ifdef LITTLE_TIMER_DIAGNOSTICS
int runDiagnostics(HINSTANCE instance, const std::wstring& directory);
#endif

} // namespace little_timer
