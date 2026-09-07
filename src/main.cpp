#include "app.h"

#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // System-DPI awareness is available on Windows 7; newer monitors are scaled
    // by Windows rather than importing APIs absent on older systems.
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) {
        MessageBoxW(nullptr, L"无法初始化图形界面，请重新启动程序。", L"小小演讲计时器", MB_OK | MB_ICONERROR);
        return 1;
    }
    int result = 0;
    {
#ifdef LITTLE_TIMER_DIAGNOSTICS
        int argc = 0;
        auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argc == 3 && std::wstring(argv[1]) == L"--self-test") {
            result = little_timer::runDiagnostics(instance, argv[2]);
            LocalFree(argv);
            Gdiplus::GdiplusShutdown(token);
            return result;
        }
        LocalFree(argv);
#endif
        little_timer::App app(instance);
        if (!app.create(show)) {
            MessageBoxW(nullptr, L"无法创建计时窗口，请关闭部分程序后重试。", L"小小演讲计时器", MB_OK | MB_ICONERROR);
            result = 1;
        } else {
            MSG message = {};
            int status;
            while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                if (app.handleKey(message)) continue;
                if (app.control.hwnd && IsDialogMessageW(app.control.hwnd, &message)) continue;
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            result = status < 0 ? 1 : static_cast<int>(message.wParam);
        }
    }
    Gdiplus::GdiplusShutdown(token);
    return result;
}
