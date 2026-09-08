#pragma once

#include "platform.h"
#include "timer.h"

namespace little_timer {
namespace ui {

constexpr COLORREF kBackground = RGB(16, 26, 42);
constexpr COLORREF kPanel = RGB(24, 38, 59);
constexpr COLORREF kHover = RGB(34, 53, 77);
constexpr COLORREF kLine = RGB(47, 65, 89);
constexpr COLORREF kText = RGB(244, 247, 253);
constexpr COLORREF kMuted = RGB(156, 176, 202);
constexpr COLORREF kBlue = RGB(166, 202, 239);
constexpr COLORREF kAmber = RGB(244, 196, 119);
constexpr COLORREF kRed = RGB(255, 128, 120);

// Paint a complete frame in memory, then copy it to the destination once.
// Keeps the destination's viewport and clipping (including child windows).
class PaintBuffer {
public:
    PaintBuffer(HDC target, const RECT& area);
    ~PaintBuffer();
    PaintBuffer(const PaintBuffer&) = delete;
    PaintBuffer& operator=(const PaintBuffer&) = delete;
    HDC dc() const { return memory_ ? memory_ : target_; }

private:
    HDC target_;
    RECT area_;
    HDC memory_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
};

void subclassButton(HWND hwnd);
Gdiplus::Color color(COLORREF value, BYTE alpha = 255);
void text(Gdiplus::Graphics& g, const std::wstring& value, Gdiplus::RectF rect,
          float size, COLORREF ink = kText, bool bold = false,
          Gdiplus::StringAlignment alignment = Gdiplus::StringAlignmentNear,
          const wchar_t* face = L"Microsoft YaHei");
void roundRect(Gdiplus::Graphics& g, Gdiplus::RectF rect, float radius,
               COLORREF fill, COLORREF stroke = 0);
void logo(Gdiplus::Graphics& g, float x, float y, float size);
void button(const DRAWITEMSTRUCT& item, bool primary = false, bool selected = false,
            float scale = 1.0f);
void checkbox(HDC dc, HWND hwnd, float scale);
void drawStage(Gdiplus::Graphics& g, Gdiplus::RectF rect, const Timer& timer,
               bool presentation, const std::wstring& notice = L"");
void drawHeader(Gdiplus::Graphics& g, float width, float scale);
void drawFooter(Gdiplus::Graphics& g, float width, float height, float scale,
                const Timer& timer, const std::wstring& notice);

} // namespace ui
} // namespace little_timer
