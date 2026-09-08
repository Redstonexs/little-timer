#include "sound.h"

#include <algorithm>
#include <cmath>

namespace little_timer {

std::vector<std::int16_t> synthesize(Sound sound, int volume) {
    constexpr double pi = 3.14159265358979323846;
    const bool gentle = sound == Sound::Gentle;
    const double duration = gentle ? 1.8 : 2.2;
    // Integer sample counts are consistent on x87 (32-bit Windows) and SSE.
    const int sampleCount = kSampleRate * (gentle ? 180 : 220) / 100;
    std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));
    const double volumeGain = std::max(0, std::min(100, volume)) / 100.0;
    if (volumeGain == 0) return samples;
    std::vector<double> waveform(samples.size());
    double peak = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double time = static_cast<double>(i) / kSampleRate;
        double value = 0;
        if (gentle) {
            // Keep the gentle two-note character, with a fuller, longer decay.
            for (int note = 0; note < 2; ++note) {
                const double t = time - note * 0.36;
                if (t < 0) continue;
                const double frequency = note == 0 ? 659.255 : 880.0;
                const double attack = std::min(1.0, t / 0.025);
                const double envelope = attack * std::exp(-2.1 * t);
                value += envelope * (std::sin(2 * pi * frequency * t)
                       + 0.16 * std::sin(2 * pi * frequency * 2 * t));
            }
        } else {
            // Two bell strikes with overlapping tails. Inharmonic partials
            // and faster-decaying high frequencies produce a metallic ring.
            struct Partial { double ratio, weight, decay; };
            constexpr Partial partials[] = {
                {1.00, 0.95, 1.5}, {2.01, 0.32, 2.2}, {2.74, 0.22, 3.0},
                {4.07, 0.12, 3.7}, {5.43, 0.06, 4.5}
            };
            for (int strike = 0; strike < 2; ++strike) {
                const double t = time - strike * 0.6;
                if (t < 0) continue;
                const double attack = std::min(1.0, t / 0.005);
                for (const auto& partial : partials)
                    value += attack * partial.weight * std::exp(-partial.decay * t)
                           * std::sin(2 * pi * 880 * partial.ratio * t);
            }
        }
        waveform[i] = value * std::min(1.0, (duration - time) / 0.12);
        peak = std::max(peak, std::abs(waveform[i]));
    }
    // Master before applying the user's volume: gain stays linear at every
    // setting and overlapping strikes never hit a hard clipping limiter.
    const double gain = (gentle ? 0.82 : 0.96) / peak * volumeGain;
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = static_cast<std::int16_t>(std::lround(waveform[i] * gain * 32767));
    return samples;
}

} // namespace little_timer
