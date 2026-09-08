#include "settings_dialog.h"
#include "ui.h"

#include <algorithm>
#include <cstdlib>

namespace little_timer {
namespace {
enum Id {
    Title = 200, Duration, Reminders, Repeat, Volume,
    ReminderSound, EndSound, Overtime, Remember, PreviewGentle, PreviewEnd,
    Preset5, Preset10, Preset15, Preset20, Preset30, Feedback
};

LRESULT CALLBACK checkboxProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                   UINT_PTR, DWORD_PTR scaleValue) {
    const float scale = static_cast<float>(scaleValue) / 1000;
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        ui::checkbox(dc, hwnd, scale);
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (msg == WM_PRINTCLIENT) { ui::checkbox(reinterpret_cast<HDC>(wp), hwnd, scale); return 0; }
    if (msg == WM_ERASEBKGND) return 1;
    const auto result = DefSubclassProc(hwnd, msg, wp, lp);
    if (msg == BM_SETCHECK || msg == WM_LBUTTONUP || msg == WM_KEYUP || msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        InvalidateRect(hwnd, nullptr, FALSE);
    return result;
}

std::wstring valueOf(HWND parent, int id) {
    wchar_t value[512] = {};
    GetDlgItemTextW(parent, id, value, 512);
    return value;
}

void label(HWND parent, const wchar_t* value, int x, int y, int w, int h, HFONT font, float s, int id = -1) {
    const auto control = CreateWindowExW(0, L"STATIC", value, WS_CHILD | WS_VISIBLE | SS_LEFT,
        static_cast<int>(x * s), static_cast<int>(y * s), static_cast<int>(w * s), static_cast<int>(h * s),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void control(HWND parent, const wchar_t* type, const wchar_t* value, int id,
             int x, int y, int w, int h, DWORD style, HFONT font, float s) {
    const bool edit = std::wstring(type) == L"EDIT";
    const auto child = CreateWindowExW(edit ? WS_EX_CLIENTEDGE : 0, type, value,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
        static_cast<int>(x * s), static_cast<int>(y * s), static_cast<int>(w * s), static_cast<int>(h * s),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (std::wstring(type) == L"BUTTON" && (style & BS_TYPEMASK) == BS_OWNERDRAW)
        ui::subclassButton(child);
    if (edit) {
        SendMessageW(child, EM_SETLIMITTEXT, id == Title ? 80 : 200, 0);
        SendMessageW(child, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
    }
}

void checkbox(HWND parent, const wchar_t* value, int id, int x, int y, int w,
              bool checked, HFONT font, float s) {
    control(parent, L"BUTTON", value, id, x, y, w, 26, BS_AUTOCHECKBOX, font, s);
    SetWindowSubclass(GetDlgItem(parent, id), checkboxProcedure, 1, static_cast<DWORD_PTR>(s * 1000));
    CheckDlgButton(parent, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

} // namespace

bool SettingsDialog::show(HWND owner, HINSTANCE instance) {
    HDC dc = GetDC(owner);
    scale = GetDeviceCaps(dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(owner, dc);
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
    // Fits 1024x768 at default scaling, and small desktops with larger system DPI.
    scale = std::max(.65f, std::min(scale, (monitor.rcWork.bottom - monitor.rcWork.top - 70) / 654.0f));
    font = CreateFontW(-static_cast<int>(14 * scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Microsoft YaHei");
    background = CreateSolidBrush(ui::kBackground);
    field = CreateSolidBrush(ui::kPanel);
    struct EmptyDialog {
        DLGTEMPLATE dialog;
        WORD menu;
        WORD windowClass;
        WORD title;
    } resource = {};
    resource.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | DS_MODALFRAME | DS_CENTER;
    resource.dialog.dwExtendedStyle = WS_EX_CONTROLPARENT;
    resource.dialog.cx = 350;
    resource.dialog.cy = 370;
    const auto result = DialogBoxIndirectParamW(instance, &resource.dialog, owner, procedure,
                                               reinterpret_cast<LPARAM>(this));
    audio.stop();
    DeleteObject(font);
    DeleteObject(background);
    DeleteObject(field);
    font = nullptr;
    return result == IDOK;
}

bool SettingsDialog::read(HWND dialog) {
    Settings parsed = value;
    parsed.title = valueOf(dialog, Title);
    int badField = 0;
    if (!parseTime(valueOf(dialog, Duration), parsed.durationSeconds)) {
        message = L"时长请填分钟或 分:秒，例如 20 或 0:30（最长 24 小时）。";
        badField = Duration;
    } else if (!parseReminders(valueOf(dialog, Reminders), parsed.durationSeconds, parsed.reminders, message)) {
        badField = Reminders;
    } else if (!parseTime(valueOf(dialog, Repeat), parsed.repeatSeconds, true)) {
        message = L"循环间隔请填分钟或 分:秒；填 0 关闭。";
        badField = Repeat;
    }
    BOOL translated = FALSE;
    parsed.volume = static_cast<int>(GetDlgItemInt(dialog, Volume, &translated, FALSE));
    if (!badField && (!translated || parsed.volume > 100)) {
        message = L"音量请输入 0 到 100 的整数。";
        badField = Volume;
    }
    parsed.reminderSound = IsDlgButtonChecked(dialog, ReminderSound) == BST_CHECKED;
    parsed.endSound = IsDlgButtonChecked(dialog, EndSound) == BST_CHECKED;
    parsed.overtime = IsDlgButtonChecked(dialog, Overtime) == BST_CHECKED;
    if (!badField && !validate(parsed, message)) badField = Repeat;
    if (badField) {
        SetDlgItemTextW(dialog, Feedback, message.c_str());
        SetFocus(GetDlgItem(dialog, badField));
        SendDlgItemMessageW(dialog, badField, EM_SETSEL, 0, -1);
        return false;
    }
    value = parsed;
    remember = IsDlgButtonChecked(dialog, Remember) == BST_CHECKED;
    return true;
}

INT_PTR CALLBACK SettingsDialog::procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<SettingsDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    if (msg == WM_INITDIALOG) {
        self = reinterpret_cast<SettingsDialog*>(lp);
        SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(self));
        SetWindowTextW(hwnd, L"计时设置");
        const float s = self->scale;
        RECT rect = {0, 0, static_cast<LONG>(570 * s), static_cast<LONG>(654 * s)};
        AdjustWindowRectEx(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE,
                          static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
        RECT owner;
        GetWindowRect(GetParent(hwnd), &owner);
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromWindow(GetParent(hwnd), MONITOR_DEFAULTTONEAREST), &monitor);
        const int width = rect.right - rect.left, height = rect.bottom - rect.top;
        const int x = std::max<int>(monitor.rcWork.left, std::min<int>((owner.left + owner.right - width) / 2, monitor.rcWork.right - width));
        const int y = std::max<int>(monitor.rcWork.top, std::min<int>((owner.top + owner.bottom - height) / 2, monitor.rcWork.bottom - height));
        SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
        const auto f = self->font;
        const auto& v = self->value;
        label(hwnd, L"设置这一轮演讲", 28, 21, 500, 25, f, s);
        label(hwnd, L"演讲标题", 28, 65, 140, 24, f, s);
        control(hwnd, L"EDIT", v.title.c_str(), Title, 28, 91, 514, 32, ES_AUTOHSCROLL, f, s);
        label(hwnd, L"演讲时长", 28, 140, 125, 24, f, s);
        control(hwnd, L"EDIT", editableTime(v.durationSeconds).c_str(), Duration, 28, 166, 112, 32, ES_AUTOHSCROLL, f, s);
        label(hwnd, L"分钟；也可填 20:30", 153, 171, 180, 24, f, s);
        const int minutes[] = {5, 10, 15, 20, 30};
        for (int i = 0; i < 5; ++i) control(hwnd, L"BUTTON", (std::to_wstring(minutes[i]) + L" 分钟").c_str(),
            Preset5 + i, 28 + i * 105, 208, 94, 31, BS_OWNERDRAW, f, s);
        label(hwnd, L"剩余时间提醒", 28, 257, 250, 24, f, s);
        control(hwnd, L"EDIT", reminderText(v.reminders).c_str(), Reminders, 28, 284, 514, 32, ES_AUTOHSCROLL, f, s);
        label(hwnd, L"例如 5, 1, 0:30 ＝ 剩余 5 分钟、1 分钟、30 秒；留空关闭", 28, 323, 520, 24, f, s);
        label(hwnd, L"循环提醒间隔", 28, 364, 120, 24, f, s);
        control(hwnd, L"EDIT", editableTime(v.repeatSeconds).c_str(), Repeat, 156, 358, 82, 32, ES_AUTOHSCROLL, f, s);
        label(hwnd, L"分钟，0 关闭", 248, 364, 116, 24, f, s);
        label(hwnd, L"音量", 380, 364, 55, 24, f, s);
        control(hwnd, L"EDIT", std::to_wstring(v.volume).c_str(), Volume, 435, 358, 68, 32, ES_NUMBER, f, s);
        label(hwnd, L"%", 514, 364, 28, 24, f, s);
        checkbox(hwnd, L"时段提醒 · 柔和双音", ReminderSound, 28, 410, 288, v.reminderSound, f, s);
        control(hwnd, L"BUTTON", L"试听提醒音", PreviewGentle, 382, 406, 160, 32, BS_OWNERDRAW, f, s);
        checkbox(hwnd, L"结束提醒 · 连续铃响", EndSound, 28, 452, 288, v.endSound, f, s);
        control(hwnd, L"BUTTON", L"试听结束音", PreviewEnd, 382, 448, 160, 32, BS_OWNERDRAW, f, s);
        checkbox(hwnd, L"到时后继续显示超时时长", Overtime, 28, 496, 500, v.overtime, f, s);
        checkbox(hwnd, L"记住设置（在 EXE 旁保存配置文件）", Remember, 28, 531, 514, self->remember, f, s);
        label(hwnd, L"", 28, 565, 514, 30, f, s, Feedback);
        control(hwnd, L"BUTTON", L"取消", IDCANCEL, 290, 605, 112, 34, BS_OWNERDRAW, f, s);
        control(hwnd, L"BUTTON", L"应用设置", IDOK, 416, 605, 126, 34, BS_OWNERDRAW, f, s);
        SetTimer(hwnd, 1, 250, nullptr);
        return TRUE;
    }
    if (!self) return FALSE;
    switch (msg) {
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (id == IDCANCEL) { EndDialog(hwnd, IDCANCEL); return TRUE; }
        if (id == IDOK) { if (self->read(hwnd)) EndDialog(hwnd, IDOK); return TRUE; }
        if (id >= Preset5 && id <= Preset30 && HIWORD(wp) == BN_CLICKED) {
            const int minutes[] = {5, 10, 15, 20, 30};
            const int duration = minutes[id - Preset5] * 60;
            SetDlgItemTextW(hwnd, Duration, editableTime(duration).c_str());
            // A preset supplies valid matching reminders rather than invalid 5/5.
            SetDlgItemTextW(hwnd, Reminders, duration <= 300 ? L"1" : L"5, 1");
            SetDlgItemTextW(hwnd, Repeat, L"0");
            return TRUE;
        }
        if ((id == PreviewGentle || id == PreviewEnd) && HIWORD(wp) == BN_CLICKED) {
            BOOL ok = FALSE;
            const UINT volume = GetDlgItemInt(hwnd, Volume, &ok, FALSE);
            std::wstring message;
            if (!ok || volume > 100) message = L"音量请输入 0 到 100 的整数。";
            else if (volume == 0) message = L"当前音量为 0；调高音量后即可试听。";
            else if (!self->audio.play(id == PreviewGentle ? Sound::Gentle : Sound::End, volume))
                message = L"无法播放声音，请检查扬声器或音频输出设备。";
            else message = id == PreviewGentle ? L"正在试听柔和提醒音…" : L"正在试听结束铃声…";
            SetDlgItemTextW(hwnd, Feedback, message.c_str());
            return TRUE;
        }
        break;
    }
    case WM_TIMER: self->audio.collect(); return TRUE;
    case WM_DRAWITEM:
        ui::button(*reinterpret_cast<DRAWITEMSTRUCT*>(lp), wp == IDOK, false, self->scale);
        return TRUE;
    case WM_CTLCOLORDLG: return reinterpret_cast<INT_PTR>(self->background);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetTextColor(reinterpret_cast<HDC>(wp), GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == Feedback ? ui::kAmber : ui::kMuted);
        SetBkColor(reinterpret_cast<HDC>(wp), ui::kBackground);
        return reinterpret_cast<INT_PTR>(self->background);
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wp), ui::kText);
        SetBkColor(reinterpret_cast<HDC>(wp), ui::kPanel);
        return reinterpret_cast<INT_PTR>(self->field);
    case WM_CLOSE: EndDialog(hwnd, IDCANCEL); return TRUE;
    case WM_DESTROY: KillTimer(hwnd, 1); self->audio.stop(); return TRUE;
    }
    return FALSE;
}

} // namespace little_timer
