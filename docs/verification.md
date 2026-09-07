# 验证说明

本项目交付 `dist/LittleTimer.exe`，为面向 Windows 7 的 32 位便携程序。本页区分已经执行的测试与尚未执行的系统验证。

## 已执行

- Linux：GCC 15.2 的 AddressSanitizer / UndefinedBehaviorSanitizer 核心测试。
- Windows：Windows 11，系统版本 `10.0.26200.9168`，运行 MinGW-w64 交叉编译的 x86 EXE。
- GitHub Actions：Ubuntu 24.04 构建与 Windows Server 2022 原生验证已在[远端工作流](https://github.com/Redstonexs/little-timer/actions/runs/34106202209)通过。
- 图形：真实 Win32 窗口与设置对话框、GDI+ 渲染，覆盖 1920×1080、1024×768、800×600、640×480 输出。
- 音频：真实 Windows 音频输出设备，使用静音 PCM 检查异步提交、打断播放、完成后释放与重新打开。
- 包装：通过 `scripts/verify-pe.py` 检查 PE32、GUI 子系统 6.1、ASLR / DEP、内置清单和系统 DLL 依赖。

计时与音效测试覆盖 84 项检查，包括暂停时的毫秒余量、倒计时取整、跨过提醒点、结束优先级、每轮只提醒一次、循环提醒、超时、输入边界、中文配置及两种音效的音量差异。

原生测试记录在 `build/windows-qa/windows-report.txt`，包含按钮和快捷键操作、独立投屏、全屏恢复、设置验证与取消、便携配置读写，以及连续重绘的 GDI 对象数量检查。实际界面截图位于 `docs/images/`。

交付产物的大小、SHA-256 和完整导入表，可重新生成：

```bash
python3 scripts/verify-pe.py dist/LittleTimer.exe --report build/pe-report.json
```

## 尚未验证

- Windows 7 实机 / 虚拟机启动、显示和音频播放。
- Windows 8 / 8.1 / 10 实机运行。
- 实际会场的双显示器布局、不同 DPI 组合，以及投影仪 / HDMI 扬声器音量。
- CMake / MSVC 构建路径。

Windows 7 兼容性目前由 x86 产物、6.1 子系统、旧版系统接口和系统 DLL 导入检查支撑。导入检查不能替代目标系统运行测试，也不对任意第三方音频驱动、精简系统或系统字体缺失作保证。

在目标 Windows 7 机器上，可直接复制 EXE，设置总时长 `0:10`、剩余提醒 `0:05, 0:02`，验证两种试听音、开始 / 暂停 / 继续、全屏、结束音及超时显示；无需安装运行库。
