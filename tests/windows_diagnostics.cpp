// Built only into LittleTimerQA.exe. Uses real Win32 windows off screen and
// the production rendering/command handlers; never records the user's desktop.
#include "app.h"
#include "settings_dialog.h"
#include "ui.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace little_timer {
namespace {
std::wostringstream report;
int failures = 0;
int checks = 0;
std::wstring outputDirectory;
int dialogPass = 0;

LRESULT CALLBACK countTextChanges(HWND hwnd, UINT message, WPARAM wp, LPARAM lp,
                                  UINT_PTR, DWORD_PTR data) {
    if (message == WM_SETTEXT) ++*reinterpret_cast<int*>(data);
    return DefSubclassProc(hwnd, message, wp, lp);
}

void check(bool condition, const wchar_t* label) {
    ++checks;
    report << (condition ? L"PASS " : L"FAIL ") << label << L"\n";
    if (!condition) ++failures;
}

bool saveBitmap(HBITMAP bitmap, const std::wstring& name) {
    Gdiplus::Bitmap image(bitmap, nullptr);
    // Built-in GDI+ PNG encoder; present on Windows 7.
    const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    return image.Save((outputDirectory + L"\\" + name).c_str(), &png, nullptr) == Gdiplus::Ok;
}

void snapshot(App& app, Window& window, int width, int height, const std::wstring& name) {
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    auto old = SelectObject(dc, bitmap);
    app.paint(window, dc, width, height);
    if (!window.audience && !window.fullscreen) {
        for (int id : {Audience, Fullscreen, OpenSettings, Mute, Reset, StartPause}) {
            const auto button = GetDlgItem(window.hwnd, id);
            RECT rect;
            GetWindowRect(button, &rect);
            MapWindowPoints(nullptr, window.hwnd, reinterpret_cast<POINT*>(&rect), 2);
            const int saved = SaveDC(dc);
            SetViewportOrgEx(dc, rect.left, rect.top, nullptr);
            DRAWITEMSTRUCT item = {};
            item.CtlType = ODT_BUTTON;
            item.CtlID = id;
            item.hwndItem = button;
            item.hDC = dc;
            item.itemAction = ODA_DRAWENTIRE;
            item.itemState = IsWindowEnabled(button) ? 0 : ODS_DISABLED;
            item.rcItem = {0, 0, rect.right - rect.left, rect.bottom - rect.top};
            SendMessageW(window.hwnd, WM_DRAWITEM, id, reinterpret_cast<LPARAM>(&item));
            RestoreDC(dc, saved);
        }
    }
    SelectObject(dc, old);
    check(saveBitmap(bitmap, name), name.c_str());
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

void setClientSize(HWND hwnd, int width, int height) {
    RECT rect = {0, 0, width, height};
    AdjustWindowRectEx(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE,
                      static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
    SetWindowPos(hwnd, nullptr, -20000, -20000, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void auditRefresh(App& app) {
    int textChanges = 0;
    const int ids[] = {Audience, Fullscreen, OpenSettings, Mute, Reset, StartPause};
    for (int id : ids)
        SetWindowSubclass(GetDlgItem(app.control.hwnd, id), countTextChanges, 99,
                          reinterpret_cast<DWORD_PTR>(&textChanges));
    for (int i = 0; i < 10; ++i) app.refresh();
    check(textChanges == 0, L"unchanged refresh does not rewrite button labels");

    textChanges = 0;
    app.command(StartPause, app.control);
    wchar_t title[64] = {};
    GetDlgItemTextW(app.control.hwnd, StartPause, title, 64);
    check(textChanges == 1 && std::wstring(title) == L"暂停",
          L"state change updates only the affected button label");
    for (int id : ids) RemoveWindowSubclass(GetDlgItem(app.control.hwnd, id), countTextChanges, 99);

    int captionChanges = 0;
    SetWindowSubclass(app.control.hwnd, countTextChanges, 99,
                      reinterpret_cast<DWORD_PTR>(&captionChanges));
    app.timer.reset();
    app.timer.start(0);
    for (int i = 1; i <= 3; ++i) app.tick(i * 1000);
    check(app.timer.displaySeconds() == 1197 && captionChanges == 0,
          L"countdown advances without repainting native window caption");
    RemoveWindowSubclass(app.control.hwnd, countTextChanges, 99);
    app.timer.reset();
    app.refresh();

    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 200, 60);
    const auto old = SelectObject(dc, bitmap);
    const RECT rect = {0, 0, 200, 60};
    HBRUSH marker = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(dc, &rect, marker);
    SendMessageW(GetDlgItem(app.control.hwnd, StartPause), WM_ERASEBKGND,
                 reinterpret_cast<WPARAM>(dc), 0);
    check(GetPixel(dc, 20, 20) == RGB(255, 0, 255),
          L"button erase does not expose an intermediate background frame");
    SetViewportOrgEx(dc, 10, 5, nullptr);
    DRAWITEMSTRUCT item = {};
    item.CtlType = ODT_BUTTON;
    item.CtlID = StartPause;
    item.hwndItem = GetDlgItem(app.control.hwnd, StartPause);
    item.hDC = dc;
    item.rcItem = {0, 0, 158, 49};
    SendMessageW(app.control.hwnd, WM_DRAWITEM, StartPause, reinterpret_cast<LPARAM>(&item));
    check(GetPixel(dc, 0, 0) == ui::kBackground && GetPixel(dc, 157, 48) == ui::kBackground,
          L"buffered button fully paints corners without dark seams");
    check(GetPixel(dc, 160, 50) == RGB(255, 0, 255),
          L"buffered button respects destination viewport and bounds");
    SelectObject(dc, old);
    DeleteObject(marker);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

void captureDialog(HWND hwnd) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    const auto old = SelectObject(dc, bitmap);
    SendMessageW(hwnd, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_NONCLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    check(GetPixel(dc, width / 2, height / 2) != RGB(0, 0, 0), L"settings capture contains rendered pixels");
    SelectObject(dc, old);
    check(saveBitmap(bitmap, L"settings.png"), L"settings dialog renders");
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

void CALLBACK auditDialog(HWND hwnd, UINT, UINT_PTR id, DWORD) {
    KillTimer(hwnd, id);
    if (dialogPass == 0) {
        captureDialog(hwnd);
        SetDlgItemTextW(hwnd, 201, L"0:10");
        SetDlgItemTextW(hwnd, 202, L"5");
        SendMessageW(hwnd, WM_COMMAND, IDOK, 0);
        check(IsWindow(hwnd), L"invalid reminders keep settings open");
        // Feedback ID is independent of wording; keyboard focus points to the bad field.
        check(GetFocus() == GetDlgItem(hwnd, 202), L"invalid reminders focus relevant input");
        SetDlgItemTextW(hwnd, 202, L"0:05, 0:02");
        SetDlgItemTextW(hwnd, 203, L"0:03");
        SetDlgItemTextW(hwnd, 204, L"0");
        SetDlgItemTextW(hwnd, 200, L"现场验证 · 中文标题");
        CheckDlgButton(hwnd, 208, BST_UNCHECKED);
        SendMessageW(hwnd, WM_COMMAND, IDOK, 0);
    } else {
        SetDlgItemTextW(hwnd, 200, L"this must be discarded");
        SendMessageW(hwnd, WM_COMMAND, IDCANCEL, 0);
    }
    // EndDialog destroys the window after this callback returns to its modal loop.
}

LRESULT CALLBACK dialogHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HCBT_ACTIVATE) {
        const auto hwnd = reinterpret_cast<HWND>(wp);
        wchar_t title[64] = {};
        GetWindowTextW(hwnd, title, 64);
        if (std::wstring(title) == L"计时设置") {
            SetWindowPos(hwnd, nullptr, -20000, -20000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            SetTimer(hwnd, 99, 20, auditDialog);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

void saveWave(const std::wstring& name, Sound sound) {
    const auto samples = synthesize(sound, 75);
    const auto file = CreateFileW((outputDirectory + L"\\" + name).c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { check(false, L"audio sample export"); return; }
    const auto write = [&](const void* data, DWORD size) {
        DWORD written = 0;
        return WriteFile(file, data, size, &written, nullptr) && written == size;
    };
    const auto u32 = [&](std::uint32_t value) { return write(&value, 4); };
    const auto u16 = [&](std::uint16_t value) { return write(&value, 2); };
    const auto bytes = static_cast<std::uint32_t>(samples.size() * 2);
    const bool ok = write("RIFF", 4) && u32(36 + bytes) && write("WAVEfmt ", 8) && u32(16)
        && u16(1) && u16(1) && u32(kSampleRate) && u32(kSampleRate * 2) && u16(2) && u16(16)
        && write("data", 4) && u32(bytes) && write(samples.data(), bytes);
    CloseHandle(file);
    check(ok, L"generated audio exported as PCM WAV");
}

} // namespace

int runDiagnostics(HINSTANCE instance, const std::wstring& directory) {
    outputDirectory = directory;
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return 2;
    App app(instance);
    app.diagnostic = true;
    check(app.create(SW_HIDE), L"real 32-bit Win32 application initializes");
    if (!app.control.hwnd) return 2;
    KillTimer(app.control.hwnd, 1);
    app.scale = 1;
    setClientSize(app.control.hwnd, 1000, 700);
    app.layout(app.control);
    auditRefresh(app);
    snapshot(app, app.control, 1000, 700, L"control.png");
    check(IsWindowEnabled(GetDlgItem(app.control.hwnd, OpenSettings)), L"settings available before start");
    SendMessageW(GetDlgItem(app.control.hwnd, StartPause), BM_CLICK, 0, 0);
    check(app.timer.state() == State::Running, L"real start button starts timer");
    check(!IsWindowEnabled(GetDlgItem(app.control.hwnd, OpenSettings)), L"running round cannot be reconfigured");
    SendMessageW(GetDlgItem(app.control.hwnd, StartPause), BM_CLICK, 0, 0);
    check(app.timer.state() == State::Paused, L"real pause button pauses timer");
    SendMessageW(GetDlgItem(app.control.hwnd, Reset), BM_CLICK, 0, 0);
    check(app.timer.state() == State::Ready, L"real reset button resets timer");
    MSG key = {};
    key.hwnd = app.control.hwnd;
    key.message = WM_KEYDOWN;
    key.wParam = VK_SPACE;
    check(app.handleKey(key) && app.timer.state() == State::Running, L"Space starts timer");
    key.lParam = 1L << 30;
    check(app.handleKey(key) && app.timer.state() == State::Running, L"held Space does not repeatedly toggle timer");
    key.lParam = 0;
    check(app.handleKey(key) && app.timer.state() == State::Paused, L"Space pauses timer");
    app.timer.reset();
    key.wParam = VK_F11;
    check(app.handleKey(key) && app.control.fullscreen, L"F11 enters clean presentation mode");
    check(!(GetWindowLongPtrW(GetDlgItem(app.control.hwnd, OpenSettings), GWL_STYLE) & WS_VISIBLE), L"presentation mode hides operator controls");
    key.wParam = VK_ESCAPE;
    check(app.handleKey(key) && !app.control.fullscreen, L"Esc restores control window");
    SendMessageW(GetDlgItem(app.control.hwnd, Mute), BM_CLICK, 0, 0);
    check(app.muted, L"mute button mutes");

    key.message = WM_SYSKEYDOWN;
    key.wParam = VK_F10;
    check(app.handleKey(key) && app.stage.hwnd, L"F10 handles Windows system-key messages");
    check(app.stage.hwnd && app.stage.hwnd != app.control.hwnd, L"independent audience window opens");
    check(GetWindow(app.stage.hwnd, GW_OWNER) == nullptr, L"audience is not hidden by minimizing controller");
    if (app.stage.fullscreen) app.toggleFullscreen(app.stage);
    RECT before, after;
    GetWindowRect(app.stage.hwnd, &before);
    app.toggleFullscreen(app.stage);
    check(app.stage.fullscreen && !(GetWindowLongPtrW(app.stage.hwnd, GWL_STYLE) & WS_CAPTION), L"fullscreen removes frame");
    app.toggleFullscreen(app.stage);
    GetWindowRect(app.stage.hwnd, &after);
    check(EqualRect(&before, &after) && !app.stage.fullscreen, L"fullscreen restores window placement");
    snapshot(app, app.stage, 1920, 1080, L"stage.png");
    snapshot(app, app.stage, 1024, 768, L"stage-4x3.png");

    app.timer.start(0);
    app.cue(app.timer.advance(15 * 60 * 1000));
    check(app.timer.warning() && app.timer.displaySeconds() == 300, L"five-minute reminder and warning stage");
    snapshot(app, app.stage, 1920, 1080, L"stage-warning.png");
    app.cue(app.timer.advance(20 * 60 * 1000 + 23000));
    check(app.timer.ended() && app.timer.displaySeconds() == 23, L"end cue and overtime stage");
    snapshot(app, app.stage, 1920, 1080, L"stage-overtime.png");
    app.timer.pause(20 * 60 * 1000 + 23000);
    snapshot(app, app.control, 1000, 700, L"control-overtime.png");

    app.timer.reset();
    app.stageNotice.clear();
    Settings longTalk;
    longTalk.durationSeconds = 86400;
    longTalk.title = L"全天会议 · 24 小时显示验证";
    app.timer.configure(longTalk);
    snapshot(app, app.stage, 800, 600, L"stage-hours.png");
    app.timer.configure(Settings());
    app.refresh();
    auto hook = SetWindowsHookExW(WH_CBT, dialogHook, nullptr, GetCurrentThreadId());
    check(hook != nullptr, L"native dialog automation hook available");
    if (hook) {
        app.openSettings();
        check(app.timer.settings().durationSeconds == 10 && app.timer.settings().reminders == std::vector<int>({5, 2})
              && app.timer.settings().repeatSeconds == 3 && app.timer.settings().volume == 0,
              L"valid settings apply through real dialog");
        const auto oldTitle = app.timer.settings().title;
        dialogPass = 1;
        app.openSettings();
        check(app.timer.settings().title == oldTitle, L"cancel preserves previous settings");
        UnhookWindowsHookEx(hook);
    }

    const Settings configured = app.timer.settings();
    app.configPath = directory + L"\\便携设置测试.ini";
    app.diagnostic = false;
    app.remember = true;
    app.save();
    check(GetFileAttributesW(app.configPath.c_str()) != INVALID_FILE_ATTRIBUTES, L"portable settings saved to chosen folder");
    app.timer.configure(Settings());
    app.load();
    check(serializeSettings(app.timer.settings()) == serializeSettings(configured), L"UTF-8 Chinese configuration restores from disk");
    app.remember = false;
    app.save();
    check(GetFileAttributesW(app.configPath.c_str()) == INVALID_FILE_ATTRIBUTES, L"disabling persistence removes app configuration");
    app.diagnostic = true;

    // Use zeroed PCM to exercise real audio buffer lifetime without audible output.
    AudioPlayer player;
    check(player.play(Sound::Gentle, 0) && !player.active(), L"muted playback does not acquire audio device");
    const auto waveDevices = waveOutGetNumDevs();
    report << L"INFO Windows audio output devices: " << waveDevices << L"\n";
    if (waveDevices) {
        check(player.playPcm(std::vector<std::int16_t>(4410, 0)) && player.active(), L"real audio device queues PCM buffer");
        check(player.playPcm(std::vector<std::int16_t>(4410, 0)), L"interrupting sound safely replaces queued buffer");
        Sleep(250);
        player.collect();
        check(!player.active(), L"completed audio releases buffer and device");
        check(player.playPcm(std::vector<std::int16_t>(4410, 0)), L"audio device can reopen after completion");
        player.stop();
        check(!player.active(), L"reset cancels active audio");
    }
    saveWave(L"gentle.wav", Sound::Gentle);
    saveWave(L"end.wav", Sound::End);

    // Warm font caches, then look for accumulating GDI objects over many paints.
    snapshot(app, app.stage, 640, 480, L"stage-small.png");
    const DWORD handlesBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    HDC screen = GetDC(nullptr);
    HDC target = CreateCompatibleDC(screen);
    HBITMAP backing = CreateCompatibleBitmap(screen, 640, 480);
    const auto old = SelectObject(target, backing);
    DRAWITEMSTRUCT button = {};
    button.hwndItem = GetDlgItem(app.control.hwnd, StartPause);
    button.hDC = target;
    button.rcItem = {0, 0, 158, 49};
    for (int i = 0; i < 120; ++i) {
        app.paint(app.stage, target, 640, 480);
        ui::button(button, true);
    }
    SelectObject(target, old);
    DeleteObject(backing);
    DeleteDC(target);
    ReleaseDC(nullptr, screen);
    const DWORD handlesAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    check(handlesAfter <= handlesBefore + 2, L"window and button repainting do not leak GDI objects");
    DestroyWindow(app.stage.hwnd);
    check(!app.stage.hwnd && app.control.hwnd, L"closing audience keeps controller alive");
    app.openAudience();
    check(app.stage.hwnd != nullptr, L"audience can reopen");
    DestroyWindow(app.control.hwnd);
    check(!app.stage.hwnd && !app.control.hwnd, L"closing application releases both windows");
    report << L"\n" << checks << L" checks, " << failures << L" failures\n";
    writeUtf8(directory + L"\\windows-report.txt", report.str());
    return failures ? 1 : 0;
}

} // namespace little_timer
