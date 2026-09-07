#pragma once

#include <cstdint>
#include <vector>

namespace little_timer {
enum class Sound { Gentle, End };
constexpr int kSampleRate = 44100;
std::vector<std::int16_t> synthesize(Sound sound, int volume);
} // namespace little_timer
