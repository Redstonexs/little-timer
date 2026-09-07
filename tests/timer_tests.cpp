#include "timer.h"
#include "sound.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace little_timer;
namespace {
int checks = 0;
void check(bool condition, const char* description) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        std::exit(1);
    }
}

double rms(const std::vector<std::int16_t>& samples) {
    double sum = 0;
    for (auto value : samples) sum += static_cast<double>(value) * value;
    return std::sqrt(sum / samples.size());
}
} // namespace

int main() {
    Settings settings;
    settings.durationSeconds = 10;
    settings.reminders = {5, 2};
    Timer timer(settings);
    check(timer.state() == State::Ready && timer.displaySeconds() == 10, "initial ready state");
    check(timer.advance(50000) == Cue::None && timer.elapsedMs() == 0, "ready time does not elapse");
    timer.start(50000);
    check(timer.advance(50001) == Cue::None && timer.displaySeconds() == 10, "ceil countdown at first millisecond");
    check(timer.advance(54999) == Cue::None && !timer.warning(), "reminder never early");
    check(timer.advance(55000) == Cue::Reminder && timer.warning(), "reminder at exact threshold");
    check(timer.advance(55001) == Cue::None, "reminder fires once");
    timer.pause(55500);
    check(timer.elapsedMs() == 5500 && timer.state() == State::Paused, "pause accounts fractional elapsed time");
    check(timer.advance(500000) == Cue::None && timer.elapsedMs() == 5500, "paused time stays fixed");
    timer.start(500000);
    check(timer.advance(502499) == Cue::None, "resume preserves fractional interval");
    check(timer.advance(502500) == Cue::Reminder, "second reminder after resume");
    check(timer.advance(504499) == Cue::None && timer.displaySeconds() == 1, "one millisecond left displays one second");
    check(timer.advance(504500) == Cue::End && timer.displaySeconds() == 0, "end sound and zero coincide");
    check(timer.state() == State::Running && timer.ended(), "overtime continues");
    check(timer.advance(505499) == Cue::None && timer.displaySeconds() == 0, "overtime floor rounding");
    check(timer.advance(505500) == Cue::None && timer.displaySeconds() == 1, "overtime at complete extra second");
    check(timer.progress() == 1 && !timer.warning(), "overtime progress saturates");
    timer.pause(506000);
    timer.start(600000);
    check(timer.advance(600100) == Cue::None, "end is not replayed on resume");
    timer.reset();
    check(timer.state() == State::Ready && !timer.ended() && timer.displaySeconds() == 10, "reset fully rearms timer");
    timer.start(0);
    check(timer.advance(10000) == Cue::End, "delayed tick crossing all reminders emits only end");
    check(timer.advance(20000) == Cue::None, "no repeated end on delayed ticks");
    timer.reset();
    timer.start(0);
    check(timer.advance(8500) == Cue::Reminder, "delayed tick coalesces crossed reminders");
    check(timer.advance(8501) == Cue::None, "coalesced reminders do not replay");
    timer.reset();
    timer.start(100);
    check(timer.advance(50) == Cue::None && timer.elapsedMs() == 0, "bad backward clock is ignored");
    check(timer.advance(1100) == Cue::None && timer.elapsedMs() == 1000, "backward tick does not replace clock anchor");
    timer.reset();
    timer.start(0);
    check(timer.pause(10000) == Cue::End && timer.state() == State::Paused, "pause crossing end still emits end cue");

    settings.overtime = false;
    timer.configure(settings);
    timer.start(0);
    check(timer.advance(60000) == Cue::End && timer.state() == State::Finished, "stop-at-zero state");
    check(timer.elapsedMs() == 10000 && timer.displaySeconds() == 0, "stop-at-zero clamps elapsed");
    timer.start(70000);
    check(timer.advance(100000) == Cue::None && timer.state() == State::Finished, "finished cannot accidentally resume");

    settings.overtime = true;
    settings.repeatSeconds = 2;
    settings.reminders = {6};
    timer.configure(settings);
    timer.start(0);
    check(timer.advance(1999) == Cue::None, "periodic reminder not early");
    check(timer.advance(2000) == Cue::Reminder, "periodic reminder boundary");
    check(timer.advance(4000) == Cue::Reminder, "simultaneous point and periodic reminder coalesce");
    check(timer.advance(4001) == Cue::None, "periodic reminder exactly once");
    check(timer.advance(10000) == Cue::End, "end overrides periodic reminder");
    check(timer.advance(12000) == Cue::None, "no periodic reminders in overtime");
    timer.reset();
    timer.start(0);
    check(timer.advance(std::numeric_limits<std::uint64_t>::max()) == Cue::End, "huge time jump saturates safely");

    int seconds = -1;
    check(parseTime(L"20", seconds) && seconds == 1200, "whole minutes");
    check(parseTime(L" 0:30 ", seconds) && seconds == 30, "seconds and whitespace");
    check(parseTime(L"1:02:03", seconds) && seconds == 3723, "hours minutes seconds");
    check(parseTime(L"24:00:00", seconds) && seconds == 86400, "24-hour maximum");
    check(parseTime(L"0", seconds, true) && seconds == 0, "zero accepted for disabled interval");
    for (const wchar_t* invalid : {L"", L"0", L"0:00", L"1:60", L"1:99:00", L"24:00:01", L"20.5", L"-5", L"1x", L"0:", L":30", L"999999999999999999999999", L"1:2:3:4"})
        check(!parseTime(invalid, seconds), "invalid duration rejected");

    std::vector<int> reminders;
    std::wstring error;
    check(parseReminders(L"1，5, 0:30, 1", 1200, reminders, error)
          && reminders == std::vector<int>({300, 60, 30}), "Chinese separators, sorting and deduplication");
    check(parseReminders(L"", 1200, reminders, error) && reminders.empty(), "blank reminders disable points");
    for (const wchar_t* invalid : {L"20", L"21", L"0", L"1,", L",1", L"1,,2", L"1,abc", L"1 2", L"1,2,3,4,5,6,7,8,9,10,11,12,13"})
        check(!parseReminders(invalid, 1200, reminders, error), "invalid reminder list rejected");
    check(formatTime(0) == L"00:00" && formatTime(59) == L"00:59", "format seconds");
    check(formatTime(3600) == L"01:00:00" && formatTime(86400) == L"24:00:00", "format hours");
    check(formatTime(-1) == L"00:00", "negative input display clamps");

    Settings saved;
    saved.title = L"中文标题 = A & B";
    saved.durationSeconds = 3723;
    saved.reminders = {300, 30, 1};
    saved.repeatSeconds = 17;
    saved.volume = 42;
    saved.reminderSound = false;
    saved.overtime = false;
    Settings restored;
    check(parseSettings(serializeSettings(saved), restored), "settings round trip parses");
    check(restored.title == saved.title && restored.durationSeconds == 3723 && restored.reminders == saved.reminders
          && restored.repeatSeconds == 17 && restored.volume == 42 && !restored.reminderSound && !restored.overtime,
          "settings round trip preserves all values and Chinese");
    check(!parseSettings(L"version=1\nduration=1\nreminders=5\n", restored), "invalid saved thresholds rejected");
    check(!parseSettings(L"version=999\n", restored), "unknown configuration version rejected");
    check(!parseSettings(L"garbage", restored), "corrupt settings rejected");
    check(!parseSettings(L"version=1\nvolume=101", restored), "invalid saved volume rejected");
    check(!parseSettings(L"version=1\novertime=maybe", restored), "invalid saved boolean rejected");
    saved.title = L"bad\ntitle";
    check(!validate(saved, error), "title cannot inject configuration lines");
    saved = Settings();
    saved.repeatSeconds = saved.durationSeconds;
    check(!validate(saved, error), "period must be shorter than duration");

    const auto gentle = synthesize(Sound::Gentle, 100);
    const auto ending = synthesize(Sound::End, 100);
    const auto quiet = synthesize(Sound::Gentle, 50);
    const auto silent = synthesize(Sound::End, 0);
    check(gentle.size() == 72765, "gentle tone duration");
    check(ending.size() == 103635, "end tone duration");
    check(rms(ending) > rms(gentle) * 1.5, "end cue materially stronger than reminder");
    check(std::abs(rms(quiet) / rms(gentle) - 0.5) < 0.001, "volume scales own samples only");
    check(std::all_of(silent.begin(), silent.end(), [](std::int16_t v) { return v == 0; }), "zero volume is truly silent");
    check(*std::max_element(ending.begin(), ending.end()) < 32767, "end samples do not clip");
    check(std::abs(gentle.front()) < 5 && std::abs(gentle.back()) < 5, "gentle cue has no edge click");
    check(std::abs(ending.front()) < 5 && std::abs(ending.back()) < 5, "end cue has no edge click");

    std::cout << "PASS: " << checks << " timing, settings and audio checks\n";
    return 0;
}
