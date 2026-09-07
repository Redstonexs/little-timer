#include "ui.h"

#include <algorithm>
#include <cmath>

using namespace Gdiplus;

namespace little_timer {
namespace ui {
namespace {

void roundedPath(GraphicsPath& path, RectF rect, float radius) {
    const float d = std::min(radius * 2, std::min(rect.Width, rect.Height));
    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.GetRight() - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.GetRight() - d, rect.GetBottom() - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
}

void digits(Graphics& g, const std::wstring& value, RectF area, COLORREF ink) {
    float units = 0;
    for (wchar_t c : value) units += c == L':' ? 0.27f : (c == L'+' ? 0.53f : 0.63f);
    const float fontSize = std::min(area.Width / units, area.Height * 0.84f);
    float x = area.X + (area.Width - units * fontSize) / 2;
    // Fixed digit cells keep the time still as it changes. No downloaded fonts.
    for (wchar_t c : value) {
        const float width = (c == L':' ? 0.27f : (c == L'+' ? 0.53f : 0.63f)) * fontSize;
        text(g, std::wstring(1, c), RectF(x - fontSize * 0.08f, area.Y,
             width + fontSize * 0.16f, area.Height), fontSize, ink, true,
             StringAlignmentCenter, L"Segoe UI");
        x += width;
    }
}

} // namespace

Color color(COLORREF value, BYTE alpha) {
    return Color(alpha, GetRValue(value), GetGValue(value), GetBValue(value));
}

void text(Graphics& g, const std::wstring& value, RectF rect, float size,
          COLORREF ink, bool bold, StringAlignment alignment, const wchar_t* face) {
    FontFamily requested(face);
    const auto* family = requested.GetLastStatus() == Ok ? &requested : FontFamily::GenericSansSerif();
    Font font(family, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    StringFormat format;
    format.SetAlignment(alignment);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetFormatFlags(StringFormatFlagsNoWrap);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush brush(color(ink));
    g.DrawString(value.c_str(), static_cast<INT>(value.size()), &font, rect, &format, &brush);
}

void roundRect(Graphics& g, RectF rect, float radius, COLORREF fill, COLORREF stroke) {
    GraphicsPath path;
    roundedPath(path, rect, radius);
    SolidBrush brush(color(fill));
    g.FillPath(&brush, &path);
    if (stroke) {
        Pen pen(color(stroke), 1.0f);
        g.DrawPath(&pen, &path);
    }
}

void logo(Graphics& g, float x, float y, float size) {
    roundRect(g, RectF(x, y, size, size), size * 0.28f, kPanel);
    Pen pen(color(kBlue), size * 0.065f);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawArc(&pen, RectF(x + size * .22f, y + size * .24f, size * .56f, size * .56f), -65, 325);
    g.DrawLine(&pen, x + size * .5f, y + size * .38f, x + size * .5f, y + size * .53f);
    g.DrawLine(&pen, x + size * .5f, y + size * .53f, x + size * .63f, y + size * .59f);
    g.DrawLine(&pen, x + size * .43f, y + size * .13f, x + size * .57f, y + size * .13f);
}

void button(const DRAWITEMSTRUCT& item, bool primary, bool selected, float scale) {
    Graphics g(item.hDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    SolidBrush background(color(kBackground));
    g.FillRectangle(&background, Rect(static_cast<INT>(item.rcItem.left), static_cast<INT>(item.rcItem.top),
                    static_cast<INT>(item.rcItem.right - item.rcItem.left), static_cast<INT>(item.rcItem.bottom - item.rcItem.top)));
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    POINT cursor;
    GetCursorPos(&cursor);
    ScreenToClient(item.hwndItem, &cursor);
    const bool hovered = PtInRect(&item.rcItem, cursor) != 0;
    const COLORREF fill = primary ? (pressed ? RGB(128, 171, 215) : kBlue)
                         : ((hovered || pressed || selected) ? kHover : kPanel);
    const COLORREF ink = disabled ? RGB(99, 115, 139) : (primary ? kBackground : kText);
    const RectF rect(1, 1, static_cast<float>(item.rcItem.right - item.rcItem.left - 2),
                          static_cast<float>(item.rcItem.bottom - item.rcItem.top - 2));
    roundRect(g, rect, 9 * scale, disabled ? kPanel : fill, focused ? kBlue : (primary ? fill : kLine));
    wchar_t label[128] = {};
    GetWindowTextW(item.hwndItem, label, 128);
    text(g, label, rect, 14 * scale, ink, primary, StringAlignmentCenter);
    if (focused) {
        Pen focus(color(primary ? kBackground : kBlue), 1.0f);
        focus.SetDashStyle(DashStyleDot);
        g.DrawRectangle(&focus, rect.X + 4, rect.Y + 4, rect.Width - 8, rect.Height - 8);
    }
}

void checkbox(HDC dc, HWND hwnd, float scale) {
    Graphics g(dc);
    g.Clear(color(kBackground));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    RECT client;
    GetClientRect(hwnd, &client);
    const float size = 16 * scale;
    const float y = (client.bottom - size) / 2;
    const bool checked = SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
    roundRect(g, RectF(1, y, size, size), 3 * scale, checked ? kBlue : kPanel, checked ? kBlue : kMuted);
    if (checked) {
        Pen pen(color(kBackground), 2 * scale);
        g.DrawLine(&pen, 4 * scale, y + 8 * scale, 7 * scale, y + 11 * scale);
        g.DrawLine(&pen, 7 * scale, y + 11 * scale, 13 * scale, y + 5 * scale);
    }
    wchar_t value[160] = {};
    GetWindowTextW(hwnd, value, 160);
    text(g, value, RectF(26 * scale, 0, client.right - 28 * scale, static_cast<float>(client.bottom)),
         14 * scale, kText);
    if (GetFocus() == hwnd) {
        Pen pen(color(kBlue));
        pen.SetDashStyle(DashStyleDot);
        g.DrawRectangle(&pen, RectF(23 * scale, 1, client.right - 25 * scale, static_cast<float>(client.bottom - 2)));
    }
}

void drawHeader(Graphics& g, float width, float scale) {
    logo(g, 30 * scale, 23 * scale, 38 * scale);
    text(g, L"小小演讲计时器", RectF(80 * scale, 19 * scale, 270 * scale, 30 * scale),
         18 * scale, kText, true);
    text(g, L"L I T T L E   T I M E R", RectF(80 * scale, 48 * scale, 230 * scale, 18 * scale),
         9 * scale, kMuted, false, StringAlignmentNear, L"Segoe UI");
    Pen divider(color(kLine));
    g.DrawLine(&divider, 30 * scale, 85 * scale, width - 30 * scale, 85 * scale);
}

void drawStage(Graphics& g, RectF rect, const Timer& timer, bool presentation,
               const std::wstring& notice) {
    const auto& settings = timer.settings();
    const bool ended = timer.ended();
    const bool paused = timer.state() == State::Paused;
    const bool ready = timer.state() == State::Ready;
    const COLORREF accent = ended ? kRed : (timer.warning() ? kAmber : kBlue);
    const COLORREF ink = ended || timer.warning() ? accent : kText;
    const float s = std::max(0.55f, std::min(rect.Width / 1000.0f, rect.Height / (presentation ? 600.0f : 460.0f)));
    const float center = rect.X + rect.Width / 2;
    const float top = rect.Y + rect.Height * (presentation ? .13f : .055f);
    std::wstring status = ready ? L"准备就绪" : (paused ? L"已暂停" : L"剩余时间");
    if (ended) status = paused ? L"超时 · 已暂停" : (settings.overtime ? L"演讲结束 · 超时" : L"演讲结束");
    else if (timer.warning() && !paused) status = L"即将结束";

    text(g, settings.title.empty() ? L"演讲计时" : settings.title,
         RectF(rect.X + rect.Width * .08f, top, rect.Width * .84f, 35 * s), 22 * s,
         kMuted, false, StringAlignmentCenter);
    const float pillY = top + 50 * s;
    const float pillWidth = ended ? 198 * s : 142 * s;
    roundRect(g, RectF(center - pillWidth / 2, pillY, pillWidth, 34 * s), 17 * s, kPanel);
    SolidBrush dot(color(paused ? kMuted : accent));
    g.FillEllipse(&dot, center - pillWidth / 2 + 17 * s, pillY + 14 * s, 6 * s, 6 * s);
    text(g, status, RectF(center - pillWidth / 2 + 27 * s, pillY, pillWidth - 36 * s, 34 * s),
         14 * s, paused ? kMuted : accent, false, StringAlignmentCenter);

    const float numberTop = pillY + 40 * s;
    const float numberHeight = std::max(70.0f, rect.GetBottom() - numberTop - 125 * s);
    const std::wstring display = (ended && settings.overtime ? L"+" : L"") + formatTime(timer.displaySeconds());
    digits(g, display, RectF(rect.X + rect.Width * .065f, numberTop,
           rect.Width * .87f, numberHeight), ink);

    const float barX = rect.X + rect.Width * .115f;
    const float barW = rect.Width * .77f;
    const float barY = rect.GetBottom() - 68 * s;
    roundRect(g, RectF(barX, barY, barW, 6 * s), 3 * s, kPanel);
    // The shrinking strip is remaining time. Markers are actual reminder boundaries.
    const float remainingWidth = barW * static_cast<float>(1.0 - timer.progress());
    if (remainingWidth > 1) roundRect(g, RectF(barX, barY, remainingWidth, 6 * s), 3 * s, accent);
    if (ended) roundRect(g, RectF(barX, barY, barW, 6 * s), 3 * s, kRed);
    for (int remaining : settings.reminders) {
        const float x = barX + barW * remaining / settings.durationSeconds;
        Pen marker(color(remaining * 1000LL >= timer.remainingMs() ? kMuted : kAmber), 2 * s);
        g.DrawLine(&marker, x, barY - 5 * s, x, barY + 11 * s);
    }
    text(g, L"已用 " + formatTime(timer.elapsedMs() / 1000),
         RectF(barX - 3 * s, barY + 17 * s, barW / 2, 24 * s), 13 * s, kMuted);
    text(g, L"总时长 " + formatTime(settings.durationSeconds),
         RectF(center, barY + 17 * s, barW / 2 + 3 * s, 24 * s), 13 * s,
         kMuted, false, StringAlignmentFar);

    if (!notice.empty()) {
        text(g, notice, RectF(rect.X + 20 * s, barY - 43 * s, rect.Width - 40 * s, 27 * s),
             14 * s, accent, false, StringAlignmentCenter);
    }
}

void drawFooter(Graphics& g, float width, float height, float scale,
                const Timer& timer, const std::wstring& notice) {
    const float y = height - 133 * scale;
    Pen divider(color(kLine));
    g.DrawLine(&divider, 30 * scale, y, width - 30 * scale, y);
    std::wstring schedule = L"本轮未设置时段提醒";
    if (!timer.settings().reminders.empty()) {
        schedule = L"剩余 ";
        for (std::size_t i = 0; i < timer.settings().reminders.size(); ++i) {
            if (i) schedule += L" / ";
            schedule += formatTime(timer.settings().reminders[i]);
        }
        schedule += L" 时提醒";
    }
    if (timer.settings().repeatSeconds > 0) schedule += L" · 每 " + formatTime(timer.settings().repeatSeconds) + L" 提醒";
    text(g, schedule, RectF(30 * scale, y + 17 * scale, width - 380 * scale, 26 * scale),
         12 * scale, kMuted);
    text(g, notice.empty() ? L"Space 开始 / 暂停     R 重置     F11 全屏     F10 投屏" : notice,
         RectF(30 * scale, height - 40 * scale, width - 60 * scale, 24 * scale),
         11 * scale, notice.empty() ? kMuted : kAmber, false, StringAlignmentCenter);
}

} // namespace ui
} // namespace little_timer
