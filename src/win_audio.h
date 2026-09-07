#pragma once

#include "platform.h"
#include "sound.h"

namespace little_timer {

class AudioPlayer {
public:
    AudioPlayer() = default;
    ~AudioPlayer() { stop(); }
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    bool play(Sound sound, int volume);
    bool playPcm(std::vector<std::int16_t> samples);
    void stop();
    void collect();
    bool active() const { return device_ != nullptr; }
private:
    HWAVEOUT device_ = nullptr;
    WAVEHDR header_ = {};
    std::vector<std::int16_t> samples_;
};

} // namespace little_timer
