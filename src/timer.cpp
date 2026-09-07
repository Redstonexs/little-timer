#include "timer.h"

#include <algorithm>
#include <cwctype>
#include <iomanip>
#include <sstream>

namespace little_timer {
namespace {

std::wstring trim(const std::wstring& text) {
    const auto first = text.find_first_not_of(L" \t\r\n\ufeff");
    if (first == std::wstring::npos) return L"";
    return text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
}

bool integer(const std::wstring& input, int& value) {
    const auto text = trim(input);
    if (text.empty() || text.size() > 8) return false;
    int result = 0;
    for (wchar_t c : text) {
        if (c < L'0' || c > L'9') return false;
        result = result * 10 + (c - L'0');
    }
    value = result;
    return true;
}

} // namespace

Timer::Timer(const Settings& settings) { configure(settings); }

void Timer::configure(const Settings& settings) {
    std::wstring error;
    settings_ = validate(settings, error) ? settings : Settings();
    std::sort(settings_.reminders.begin(), settings_.reminders.end(), std::greater<int>());
    settings_.reminders.erase(std::unique(settings_.reminders.begin(), settings_.reminders.end()),
                              settings_.reminders.end());
    reset();
}

void Timer::reset() {
    state_ = State::Ready;
    elapsedMs_ = 0;
    lastTick_ = 0;
    endFired_ = false;
}

void Timer::start(std::uint64_t now) {
    if (state_ == State::Running || state_ == State::Finished) return;
    lastTick_ = now;
    state_ = State::Running;
}

Cue Timer::pause(std::uint64_t now) {
    if (state_ != State::Running) return Cue::None;
    const auto cue = advance(now);
    if (state_ == State::Running) state_ = State::Paused;
    return cue;
}

Cue Timer::advance(std::uint64_t now) {
    if (state_ != State::Running || now < lastTick_) return Cue::None;
    const std::int64_t previous = elapsedMs_;
    // Saturate after a year: prevents overflow even with a synthetic bad clock.
    constexpr std::uint64_t cap = 366ULL * 24 * 3600 * 1000;
    const auto delta = std::min(now - lastTick_, cap);
    elapsedMs_ = std::min<std::int64_t>(elapsedMs_ + static_cast<std::int64_t>(delta), cap);
    lastTick_ = now;
    const std::int64_t duration = settings_.durationSeconds * 1000LL;

    // End always wins when a stalled UI or wake crosses multiple boundaries.
    if (!endFired_ && elapsedMs_ >= duration) {
        endFired_ = true;
        if (!settings_.overtime) {
            elapsedMs_ = duration;
            state_ = State::Finished;
        }
        return Cue::End;
    }
    if (endFired_) return Cue::None;

    bool remind = false;
    for (int remaining : settings_.reminders) {
        const auto boundary = duration - remaining * 1000LL;
        if (previous < boundary && elapsedMs_ >= boundary) remind = true;
    }
    if (settings_.repeatSeconds > 0) {
        const auto period = settings_.repeatSeconds * 1000LL;
        if (elapsedMs_ / period > previous / period) remind = true;
    }
    return remind ? Cue::Reminder : Cue::None;
}

std::int64_t Timer::remainingMs() const {
    return settings_.durationSeconds * 1000LL - elapsedMs_;
}

std::int64_t Timer::displaySeconds() const {
    const auto remaining = remainingMs();
    // Countdown uses ceil so 00:00 and the end cue occur at the same instant.
    // Overtime uses floor so +00:01 appears only after a complete extra second.
    return remaining > 0 ? (remaining + 999) / 1000 : (-remaining) / 1000;
}

bool Timer::warning() const {
    if (endFired_ || settings_.reminders.empty()) return false;
    const int first = *std::max_element(settings_.reminders.begin(), settings_.reminders.end());
    return remainingMs() <= first * 1000LL;
}

double Timer::progress() const {
    return std::min(1.0, static_cast<double>(elapsedMs_) / (settings_.durationSeconds * 1000.0));
}

int Timer::nextReminder() const {
    int result = 0;
    for (int remaining : settings_.reminders)
        if (remaining * 1000LL < remainingMs()) result = std::max(result, remaining);
    return result;
}

bool parseTime(const std::wstring& input, int& seconds, bool allowZero) {
    const auto text = trim(input);
    if (text.empty()) return false;
    std::vector<int> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(L':', start);
        int part = 0;
        if (!integer(text.substr(start, end == std::wstring::npos ? end : end - start), part)) return false;
        parts.push_back(part);
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    if (parts.size() > 3) return false;
    std::int64_t total = 0;
    if (parts.size() == 1) total = parts[0] * 60LL;
    else {
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i > 0 && parts[i] >= 60) return false;
            total = total * 60 + parts[i];
        }
    }
    if (total < (allowZero ? 0 : 1) || total > kMaxDurationSeconds) return false;
    seconds = static_cast<int>(total);
    return true;
}

bool parseReminders(const std::wstring& input, int duration,
                    std::vector<int>& reminders, std::wstring& error) {
    auto text = trim(input);
    for (auto& c : text) if (c == L'，' || c == L'、' || c == L';' || c == L'；') c = L',';
    std::vector<int> parsed;
    if (!text.empty()) {
        std::size_t start = 0;
        while (start <= text.size()) {
            const auto end = text.find(L',', start);
            int seconds = 0;
            if (!parseTime(text.substr(start, end == std::wstring::npos ? end : end - start), seconds)) {
                error = L"提醒点请用逗号分隔，例如 5, 1, 0:30。";
                return false;
            }
            if (seconds >= duration) {
                error = L"每个剩余时间提醒点都必须小于演讲总时长。";
                return false;
            }
            parsed.push_back(seconds);
            if (parsed.size() > kMaxReminders) {
                error = L"最多设置 12 个提醒点。";
                return false;
            }
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
    }
    std::sort(parsed.begin(), parsed.end(), std::greater<int>());
    parsed.erase(std::unique(parsed.begin(), parsed.end()), parsed.end());
    reminders = parsed;
    error.clear();
    return true;
}

bool validate(const Settings& settings, std::wstring& error) {
    if (settings.durationSeconds < 1 || settings.durationSeconds > kMaxDurationSeconds) {
        error = L"演讲时长需在 0:01 到 24:00:00 之间。";
        return false;
    }
    if (settings.title.size() > 80 || settings.title.find_first_of(L"\r\n") != std::wstring::npos) {
        error = L"演讲标题最多 80 个字符，不能换行。";
        return false;
    }
    if (settings.reminders.size() > kMaxReminders) {
        error = L"最多设置 12 个提醒点。";
        return false;
    }
    for (int reminder : settings.reminders) {
        if (reminder <= 0 || reminder >= settings.durationSeconds) {
            error = L"提醒点必须大于零且小于演讲总时长。";
            return false;
        }
    }
    if (settings.repeatSeconds < 0 || settings.repeatSeconds >= settings.durationSeconds) {
        error = L"循环提醒间隔必须小于演讲总时长；填 0 关闭。";
        return false;
    }
    if (settings.volume < 0 || settings.volume > 100) {
        error = L"音量需在 0 到 100 之间。";
        return false;
    }
    error.clear();
    return true;
}

std::wstring formatTime(std::int64_t seconds) {
    seconds = std::max<std::int64_t>(0, seconds);
    std::wostringstream out;
    out << std::setfill(L'0');
    if (seconds >= 3600) out << std::setw(2) << seconds / 3600 << L':';
    out << std::setw(2) << (seconds / 60) % 60 << L':' << std::setw(2) << seconds % 60;
    return out.str();
}

std::wstring editableTime(int seconds) {
    return seconds % 60 == 0 ? std::to_wstring(seconds / 60)
                            : std::to_wstring(seconds / 60) + L":" + (seconds % 60 < 10 ? L"0" : L"") + std::to_wstring(seconds % 60);
}

std::wstring reminderText(const std::vector<int>& seconds) {
    std::wstring text;
    for (int value : seconds) {
        if (!text.empty()) text += L", ";
        text += editableTime(value);
    }
    return text;
}

std::wstring serializeSettings(const Settings& settings) {
    std::wostringstream out;
    out << L"[LittleTimer]\nversion=1\ntitle=" << settings.title
        << L"\nduration=" << editableTime(settings.durationSeconds)
        << L"\nreminders=" << reminderText(settings.reminders)
        << L"\nrepeat=" << editableTime(settings.repeatSeconds)
        << L"\nvolume=" << settings.volume
        << L"\nreminderSound=" << settings.reminderSound
        << L"\nendSound=" << settings.endSound
        << L"\novertime=" << settings.overtime << L"\n";
    return out.str();
}

bool parseSettings(const std::wstring& text, Settings& settings) {
    Settings parsed;
    std::wstring reminders = L"5, 1";
    std::wistringstream stream(text);
    std::wstring line;
    bool sawVersion = false;
    while (std::getline(stream, line)) {
        const auto split = line.find(L'=');
        if (split == std::wstring::npos) continue;
        const auto key = trim(line.substr(0, split));
        const auto value = trim(line.substr(split + 1));
        if (key == L"version") { if (value != L"1") return false; sawVersion = true; }
        else if (key == L"title") parsed.title = value;
        else if (key == L"duration") { if (!parseTime(value, parsed.durationSeconds)) return false; }
        else if (key == L"reminders") reminders = value;
        else if (key == L"repeat") { if (!parseTime(value, parsed.repeatSeconds, true)) return false; }
        else if (key == L"volume") { if (!integer(value, parsed.volume)) return false; }
        else if (key == L"reminderSound" || key == L"endSound" || key == L"overtime") {
            if (value != L"0" && value != L"1") return false;
            const bool enabled = value == L"1";
            if (key == L"reminderSound") parsed.reminderSound = enabled;
            else if (key == L"endSound") parsed.endSound = enabled;
            else parsed.overtime = enabled;
        }
    }
    std::wstring error;
    if (!sawVersion || !parseReminders(reminders, parsed.durationSeconds, parsed.reminders, error) || !validate(parsed, error)) return false;
    settings = parsed;
    return true;
}

} // namespace little_timer
