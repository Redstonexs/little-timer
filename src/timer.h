#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace little_timer {

constexpr int kMaxDurationSeconds = 24 * 60 * 60;
constexpr int kMaxReminders = 12;

struct Settings {
    std::wstring title = L"主题演讲";
    int durationSeconds = 20 * 60;
    std::vector<int> reminders = {5 * 60, 60}; // Remaining time, seconds.
    int repeatSeconds = 0;                    // Elapsed-time interval; 0 disables.
    int volume = 75;
    bool reminderSound = true;
    bool endSound = true;
    bool overtime = true;
};

enum class State { Ready, Running, Paused, Finished };
enum class Cue { None, Reminder, End };

// The caller supplies a monotonic millisecond clock. No GUI or wall clock here.
class Timer {
public:
    explicit Timer(const Settings& settings = Settings());
    void configure(const Settings& settings);
    void reset();
    void start(std::uint64_t now);
    Cue pause(std::uint64_t now);
    Cue advance(std::uint64_t now);

    State state() const { return state_; }
    const Settings& settings() const { return settings_; }
    std::int64_t elapsedMs() const { return elapsedMs_; }
    std::int64_t remainingMs() const;
    std::int64_t displaySeconds() const;
    bool ended() const { return endFired_; }
    bool warning() const;
    double progress() const;
    int nextReminder() const;

private:
    Settings settings_;
    State state_ = State::Ready;
    std::int64_t elapsedMs_ = 0;
    std::uint64_t lastTick_ = 0;
    bool endFired_ = false;
};

// Minutes or m:ss. Also accepts h:mm:ss; no fractions, signs, or trailing junk.
bool parseTime(const std::wstring& text, int& seconds, bool allowZero = false);
bool parseReminders(const std::wstring& text, int duration,
                    std::vector<int>& reminders, std::wstring& error);
bool validate(const Settings& settings, std::wstring& error);
std::wstring formatTime(std::int64_t seconds);
std::wstring editableTime(int seconds);
std::wstring reminderText(const std::vector<int>& seconds);

// UTF-8 is handled at the file boundary. Pure text parsing is platform independent.
std::wstring serializeSettings(const Settings& settings);
bool parseSettings(const std::wstring& text, Settings& settings);

} // namespace little_timer
