#include "app.h"
#include "settings_dialog.h"
#include "ui.h"

#include <algorithm>
#include <cmath>

namespace little_timer {
namespace {
constexpr wchar_t kWindowClass[] = L"LittleTimer.NativeWindow.1";

std::wstring readUtf8(const std::wstring& path) {
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return L"";
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 1 || size.QuadPart > 16384) {
        CloseHandle(file);
        return L"";
    }
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD count = 0;
    const bool ok = ReadFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &count, nullptr) && count == bytes.size();
    CloseHandle(file);
    if (!ok) return L"";
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), count, nullptr, 0);
    if (!length) return L"";
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), count, &result[0], length);
    return result;
}

void updateButtonText(HWND parent, int id, const wchar_t* title) {
    wchar_t current[128] = {};
    GetDlgItemTextW(parent, id, current, 128);
    if (std::wstring(current) != title) SetDlgItemTextW(parent, id, title);
}

void makeButton(HWND parent, HINSTANCE instance, int id, const wchar_t* title) {
    const auto hwnd = CreateWindowExW(0, L"BUTTON", title, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 100, 40, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    ui::subclassButton(hwnd);
}

BOOL CALLBACK collectMonitors(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    reinterpret_cast<std::vector<HMONITOR>*>(data)->push_back(monitor);
    return TRUE;
}

} // namespace

std::wstring executablePath() {
    std::wstring path(32768, L'\0');
    const auto count = GetModuleFileNameW(nullptr, &path[0], static_cast<DWORD>(path.size()));
    if (!count || count == path.size()) return L"";
    path.resize(count);
    return path;
}

bool writeUtf8(const std::wstring& path, const std::wstring& value) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!length) return false;
    std::string bytes(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &bytes[0], length, nullptr, nullptr);
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
                 && written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    return ok;
}

App::App(HINSTANCE module) : instance(module) {
    control.app = this;
    stage.app = this;
    stage.audience = true;
    configPath = executablePath();
    const auto separator = configPath.find_last_of(L"\\/");
    configPath = configPath.substr(0, separator + 1) + L"LittleTimer.ini";
}

App::~App() {
    audio.stop();
    if (keepAwake) SetThreadExecutionState(ES_CONTINUOUS);
}

void App::load() {
    if (diagnostic) return;
    const auto attributes = GetFileAttributesW(configPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return;
    Settings settings;
    if (parseSettings(readUtf8(configPath), settings)) {
        timer.configure(settings);
        remember = true;
    } else {
        notice = L"配置文件无法读取，已使用默认设置。可在设置中重新保存。";
    }
}

void App::save() {
    if (diagnostic) return;
    if (!remember) {
        if (GetFileAttributesW(configPath.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(configPath.c_str()))
            notice = L"本轮设置已应用；旧配置无法移除，下次启动仍可能读取旧设置。";
        return;
    }
    const auto temporary = configPath + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    if (!writeUtf8(temporary, serializeSettings(timer.settings()))
        || !MoveFileExW(temporary.c_str(), configPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        notice = L"本轮设置已应用，但无法保存。请把 EXE 放在可写入的文件夹。";
    }
}

bool App::create(int show) {
    WNDCLASSEXW klass = {};
    klass.cbSize = sizeof(klass);
    klass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    klass.lpfnWndProc = procedure;
    klass.hInstance = instance;
    klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    klass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    klass.hIconSm = klass.hIcon;
    klass.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    HDC dc = GetDC(nullptr);
    scale = GetDeviceCaps(dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(nullptr, dc);
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    POINT cursor;
    GetCursorPos(&cursor);
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor);
    const int availableWidth = monitor.rcWork.right - monitor.rcWork.left;
    const int availableHeight = monitor.rcWork.bottom - monitor.rcWork.top;
    scale = std::max(.65f, std::min(scale, std::min((availableWidth - 40) / 1000.0f, (availableHeight - 70) / 700.0f)));
    RECT rect = {0, 0, static_cast<LONG>(1000 * scale), static_cast<LONG>(700 * scale)};
    AdjustWindowRectEx(&rect, control.style, FALSE, WS_EX_CONTROLPARENT);
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    const auto hwnd = CreateWindowExW(WS_EX_CONTROLPARENT, kWindowClass, L"小小演讲计时器 · Little Timer",
        control.style, monitor.rcWork.left + (availableWidth - width) / 2,
        monitor.rcWork.top + (availableHeight - height) / 2,
        width, height, nullptr, nullptr, instance, &control);
    if (!hwnd) return false;
    load();
    refresh();
    if (!SetTimer(hwnd, 1, 50, nullptr)) {
        DestroyWindow(hwnd);
        return false;
    }
    ShowWindow(hwnd, show);
    if (show != SW_HIDE) UpdateWindow(hwnd);
    return true;
}

void App::layout(Window& window) {
    if (window.audience) return;
    RECT rect;
    GetClientRect(window.hwnd, &rect);
    const float w = static_cast<float>(rect.right), h = static_cast<float>(rect.bottom);
    const float s = scale;
    struct Placement { int id; float x, y, width, height; };
    const Placement positions[] = {
        {Audience, w - 334 * s, 27 * s, 102 * s, 36 * s},
        {Fullscreen, w - 220 * s, 27 * s, 92 * s, 36 * s},
        {OpenSettings, w - 116 * s, 27 * s, 86 * s, 36 * s},
        {Mute, 30 * s, h - 82 * s, 112 * s, 35 * s},
        {Reset, w - 294 * s, h - 105 * s, 94 * s, 49 * s},
        {StartPause, w - 188 * s, h - 105 * s, 158 * s, 49 * s}
    };
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE
                     | (window.fullscreen ? SWP_HIDEWINDOW : SWP_SHOWWINDOW);
    // Move/show all controls together; MoveWindow(TRUE) painted each separately.
    HDWP batch = BeginDeferWindowPos(6);
    for (const auto& p : positions) {
        if (!batch) break;
        batch = DeferWindowPos(batch, GetDlgItem(window.hwnd, p.id), nullptr,
            static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(p.width), static_cast<int>(p.height), flags);
    }
    if (batch && EndDeferWindowPos(batch)) return;
    // Allocation failure must still leave the controls usable.
    for (const auto& p : positions)
        SetWindowPos(GetDlgItem(window.hwnd, p.id), nullptr, static_cast<int>(p.x), static_cast<int>(p.y),
                     static_cast<int>(p.width), static_cast<int>(p.height), flags);
}

void App::paint(Window& window, HDC dc, int width, int height) {
    if (width < 1 || height < 1) return;
    using namespace Gdiplus;
    // GDI backbuffer avoids dependency on a GPU, DirectX, or a browser runtime.
    ui::PaintBuffer buffer(dc, RECT{0, 0, width, height});
    {
        Graphics g(buffer.dc());
        g.Clear(ui::color(ui::kBackground));
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        const bool presentation = window.audience || window.fullscreen;
        if (presentation) {
            ui::drawStage(g, RectF(0, 0, static_cast<float>(width), static_cast<float>(height)), timer, true, stageNotice);
        } else {
            ui::drawHeader(g, static_cast<float>(width), scale);
            ui::drawStage(g, RectF(0, 96 * scale, static_cast<float>(width), height - 242 * scale), timer, false, stageNotice);
            ui::drawFooter(g, static_cast<float>(width), static_cast<float>(height), scale, timer, notice);
        }
    }
}

void App::refresh() {
    if (control.hwnd) {
        std::wstring title = timer.state() == State::Running ? L"暂停" : (timer.state() == State::Paused ? L"继续计时" : L"开始计时");
        if (timer.state() == State::Finished) title = L"再来一轮";
        updateButtonText(control.hwnd, StartPause, title.c_str());
        updateButtonText(control.hwnd, Mute, muted ? L"声音已静音" : L"声音已开启");
        updateButtonText(control.hwnd, Audience, stage.hwnd ? L"关闭投屏" : L"打开投屏");
        const auto settings = GetDlgItem(control.hwnd, OpenSettings);
        const bool enabled = timer.state() != State::Running && !settingsOpen;
        if ((IsWindowEnabled(settings) != FALSE) != enabled) EnableWindow(settings, enabled);
        InvalidateRect(control.hwnd, nullptr, FALSE);
    }
    if (stage.hwnd) InvalidateRect(stage.hwnd, nullptr, FALSE);
}

void App::cue(Cue event) {
    if (event == Cue::None) return;
    const auto& settings = timer.settings();
    const bool ending = event == Cue::End;
    stageNotice = ending ? L"时间到，请结束演讲" : L"时间提醒 · 请留意演讲进度";
    stageNoticeUntil = GetTickCount64() + (ending ? 10000 : 5000);
    if (!muted && (ending ? settings.endSound : settings.reminderSound)) {
        if (!audio.play(ending ? Sound::End : Sound::Gentle, settings.volume))
            notice = L"声音播放失败，请检查扬声器或音频输出设备。";
    }
    refresh();
}

void App::updateAwake() {
    const bool running = timer.state() == State::Running;
    if (running == keepAwake || diagnostic) return;
    if (SetThreadExecutionState(ES_CONTINUOUS | (running ? ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED : 0))) {
        keepAwake = running;
    } else if (running) {
        notice = L"未能保持屏幕常亮，请检查 Windows 电源设置。";
    }
}

void App::tick(std::uint64_t now) {
    cue(timer.advance(now));
    updateAwake();
    audio.collect();
    if (!stageNotice.empty() && now >= stageNoticeUntil) {
        stageNotice.clear();
        refresh();
    }
    const auto second = timer.elapsedMs() / 1000;
    if (second != lastPaintSecond) {
        lastPaintSecond = second;
        // Keep native captions steady, especially with Windows 7 Basic themes.
        // The buffered client area displays the live time in both windows.
        refresh();
    }
}

bool App::confirmReset() {
    if (timer.state() == State::Ready || timer.state() == State::Finished || diagnostic) return true;
    const bool running = timer.state() == State::Running;
    if (running) cue(timer.pause(GetTickCount64()));
    updateAwake();
    refresh();
    const int result = MessageBoxW(control.hwnd, L"重置会清空这一轮的计时进度。确定重新开始吗？",
                                   L"重置计时", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (result != IDOK && running) timer.start(GetTickCount64());
    updateAwake();
    refresh();
    return result == IDOK;
}

void App::command(int id, Window& source) {
    if (settingsOpen) return;
    switch (id) {
    case StartPause:
        if (timer.state() == State::Running) cue(timer.pause(GetTickCount64()));
        else {
            if (timer.state() == State::Finished) {
                timer.reset();
                audio.stop();
                stageNotice.clear();
            }
            timer.start(GetTickCount64());
        }
        updateAwake();
        break;
    case Reset:
        if (confirmReset()) {
            timer.reset();
            audio.stop();
            stageNotice.clear();
            updateAwake();
        }
        break;
    case OpenSettings: openSettings(); break;
    case Fullscreen: toggleFullscreen(source); break;
    case Audience:
        if (stage.hwnd) DestroyWindow(stage.hwnd);
        else openAudience();
        break;
    case Mute:
        muted = !muted;
        if (muted) audio.stop();
        break;
    }
    refresh();
}

void App::openSettings() {
    if (settingsOpen || timer.state() == State::Running) return;
    // Applying settings starts a new round, so preserve a paused round on Cancel.
    SettingsDialog dialog;
    dialog.value = timer.settings();
    dialog.remember = remember;
    settingsOpen = true;
    refresh();
    if (dialog.show(control.hwnd, instance)) {
        if (timer.elapsedMs() == 0 || timer.state() == State::Finished || diagnostic
            || MessageBoxW(control.hwnd, L"应用设置会清空已暂停的计时进度。确定开始新一轮吗？",
                           L"应用计时设置", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) == IDOK) {
            timer.configure(dialog.value);
            remember = dialog.remember;
            audio.stop();
            stageNotice.clear();
            notice.clear();
            save();
            updateAwake();
        }
    }
    settingsOpen = false;
    refresh();
}

void App::openAudience() {
    if (stage.hwnd) return;
    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitors, reinterpret_cast<LPARAM>(&monitors));
    const auto current = MonitorFromWindow(control.hwnd, MONITOR_DEFAULTTONEAREST);
    auto target = current;
    for (auto monitor : monitors) if (monitor != current) { target = monitor; break; }
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(target, &info)) return;
    const bool secondScreen = target != current;
    stage.fullscreen = false;
    stage.style = WS_OVERLAPPEDWINDOW;
    const int width = std::min(960, static_cast<int>(info.rcWork.right - info.rcWork.left - 80));
    const int height = std::min(600, static_cast<int>(info.rcWork.bottom - info.rcWork.top - 80));
    const auto hwnd = CreateWindowExW(0, kWindowClass, L"演讲投屏 · 双击全屏 · Esc 退出",
        stage.style, info.rcWork.left + 40, info.rcWork.top + 40, width, height,
        nullptr, nullptr, instance, &stage);
    if (!hwnd) { notice = L"无法创建投屏窗口，请重试。"; return; }
    if (!diagnostic) ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    if (secondScreen) toggleFullscreen(stage);
    else notice = L"投屏窗口已打开：拖到大屏后双击全屏，Esc 退出。";
    if (!diagnostic) SetForegroundWindow(control.hwnd);
    refresh();
}

void App::toggleFullscreen(Window& window) {
    if (!window.hwnd) return;
    if (!window.fullscreen) {
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        if (!GetMonitorInfoW(MonitorFromWindow(window.hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) return;
        window.placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(window.hwnd, &window.placement);
        window.style = static_cast<DWORD>(GetWindowLongPtrW(window.hwnd, GWL_STYLE));
        window.fullscreen = true;
        SetWindowLongPtrW(window.hwnd, GWL_STYLE, window.style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW));
        SetWindowPos(window.hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);
    } else {
        window.fullscreen = false;
        SetWindowLongPtrW(window.hwnd, GWL_STYLE, window.style);
        SetWindowPlacement(window.hwnd, &window.placement);
        SetWindowPos(window.hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    layout(window);
    InvalidateRect(window.hwnd, nullptr, FALSE);
}

bool App::handleKey(MSG& message) {
    if (settingsOpen) return false;
    const bool systemF10 = message.wParam == VK_F10 && GetKeyState(VK_MENU) >= 0;
    if (message.message != WM_KEYDOWN && !(systemF10 && (message.message == WM_SYSKEYDOWN || message.message == WM_SYSKEYUP))) return false;
    const auto root = GetAncestor(message.hwnd, GA_ROOT);
    Window* source = root == stage.hwnd ? &stage : (root == control.hwnd ? &control : nullptr);
    if (!source) return false;
    if (message.message == WM_SYSKEYUP) return true;
    const auto key = message.wParam;
    if (key == VK_F11 || key == VK_F10 || key == VK_SPACE || key == 'R' || key == 'M') {
        // Ignore key autorepeat to prevent start/pause or fullscreen oscillation.
        if (message.lParam & (1L << 30)) return true;
        if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) return false;
        const int id = key == VK_F11 ? Fullscreen : (key == VK_F10 ? Audience : (key == VK_SPACE ? StartPause : (key == 'R' ? Reset : Mute)));
        command(id, *source);
        return true;
    }
    if (key == VK_OEM_COMMA && GetKeyState(VK_CONTROL) < 0) {
        if (!(message.lParam & (1L << 30))) command(OpenSettings, *source);
        return true;
    }
    if (key == VK_ESCAPE) {
        if (source->fullscreen) toggleFullscreen(*source);
        else if (source->audience) DestroyWindow(source->hwnd);
        return true;
    }
    return false;
}

LRESULT CALLBACK App::procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        window = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        window->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    if (!window || !window->app) return DefWindowProcW(hwnd, msg, wp, lp);
    auto& app = *window->app;
    switch (msg) {
    case WM_CREATE:
        if (!window->audience) {
            makeButton(hwnd, app.instance, Audience, L"打开投屏");
            makeButton(hwnd, app.instance, Fullscreen, L"全屏 F11");
            makeButton(hwnd, app.instance, OpenSettings, L"设置");
            makeButton(hwnd, app.instance, Mute, L"声音已开启");
            makeButton(hwnd, app.instance, Reset, L"重置");
            makeButton(hwnd, app.instance, StartPause, L"开始计时");
        }
        return 0;
    case WM_SIZE: app.layout(*window); InvalidateRect(hwnd, nullptr, FALSE); return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMinTrackSize.x = static_cast<LONG>((window->audience ? 440 : 740) * app.scale);
        info->ptMinTrackSize.y = static_cast<LONG>((window->audience ? 320 : 570) * app.scale);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        const auto dc = BeginPaint(hwnd, &paint);
        RECT rect;
        GetClientRect(hwnd, &rect);
        app.paint(*window, dc, rect.right, rect.bottom);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_PRINTCLIENT: {
        RECT rect;
        GetClientRect(hwnd, &rect);
        app.paint(*window, reinterpret_cast<HDC>(wp), rect.right, rect.bottom);
        return 0;
    }
    case WM_DRAWITEM:
        ui::button(*reinterpret_cast<DRAWITEMSTRUCT*>(lp), wp == StartPause, wp == Mute && app.muted, app.scale);
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) app.command(LOWORD(wp), *window);
        return 0;
    case WM_TIMER:
        if (!window->audience) app.tick(GetTickCount64());
        return 0;
    case WM_LBUTTONDBLCLK: app.toggleFullscreen(*window); return 0;
    case WM_SETCURSOR:
        if (window->fullscreen && app.timer.state() == State::Running && LOWORD(lp) == HTCLIENT) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    case WM_SYSCOMMAND:
        if (app.timer.state() == State::Running && ((wp & 0xfff0) == SC_SCREENSAVE || (wp & 0xfff0) == SC_MONITORPOWER)) return 0;
        break;
    case WM_DISPLAYCHANGE:
        if (window->fullscreen) {
            MONITORINFO monitor = {};
            monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                SetWindowPos(hwnd, nullptr, monitor.rcMonitor.left, monitor.rcMonitor.top,
                    monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_CLOSE:
        if (!window->audience && !app.diagnostic && (app.timer.state() == State::Running || app.timer.state() == State::Paused)) {
            // Do not pause a live speech while the operator decides whether to exit.
            if (MessageBoxW(hwnd, L"当前演讲还未重置。确定关闭计时器和投屏窗口吗？",
                L"关闭计时器", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (window->audience) {
            window->hwnd = nullptr;
            window->fullscreen = false;
            app.refresh();
        } else {
            KillTimer(hwnd, 1);
            if (app.stage.hwnd) DestroyWindow(app.stage.hwnd);
            app.audio.stop();
            SetThreadExecutionState(ES_CONTINUOUS);
            app.keepAwake = false;
            window->hwnd = nullptr;
            PostQuitMessage(0);
        }
        return 0;
    case WM_NCDESTROY: SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace little_timer
