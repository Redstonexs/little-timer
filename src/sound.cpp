#include "sound.h"

#include <algorithm>
#include <cmath>

namespace little_timer {

std::vector<std::int16_t> synthesize(Sound sound, int volume) {
    constexpr double pi = 3.14159265358979323846;
    const bool gentle = sound == Sound::Gentle;
    const double duration = gentle ? 1.65 : 2.35;
    // Integer sample counts are consistent on x87 (32-bit Windows) and SSE.
    const int sampleCount = kSampleRate * (gentle ? 165 : 235) / 100;
    std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));
    const double gain = std::max(0, std::min(100, volume)) / 100.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double time = static_cast<double>(i) / kSampleRate;
        double value = 0;
        if (gentle) {
            // Two soft, decaying bell notes. Short fades avoid clicks.
            for (int note = 0; note < 2; ++note) {
                const double t = time - note * 0.25;
                if (t < 0) continue;
                const double frequency = note == 0 ? 659.255 : 880.0;
                const double attack = std::min(1.0, t / 0.025);
                const double envelope = attack * std::exp(-3.7 * t);
                value += 0.28 * envelope * (std::sin(2 * pi * frequency * t)
                       + 0.20 * std::sin(2 * pi * frequency * 2.01 * t));
            }
        } else {
            // Three separated, fuller two-tone chimes: unmistakable, no harsh buzzer.
            for (int note = 0; note < 3; ++note) {
                const double t = time - note * 0.62;
                const double length = note == 2 ? 0.95 : 0.46;
                if (t < 0 || t > length) continue;
                const double envelope = std::min(1.0, t / 0.016)
                    * std::min(1.0, (length - t) / 0.12) * std::exp(-0.85 * t);
                value += 0.56 * envelope * (0.72 * std::sin(2 * pi * 783.991 * t)
                       + 0.28 * std::sin(2 * pi * 523.251 * t));
            }
        }
        const double tail = std::min(1.0, (duration - time) / 0.04);
        value = std::max(-0.98, std::min(0.98, value * gain * tail));
        samples[i] = static_cast<std::int16_t>(std::lround(value * 32767));
    }
    return samples;
}

} // namespace little_timer
