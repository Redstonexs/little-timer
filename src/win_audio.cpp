#include "win_audio.h"

namespace little_timer {

void AudioPlayer::stop() {
    if (!device_) return;
    // Reset returns queued buffers before their storage is released.
    waveOutReset(device_);
    waveOutUnprepareHeader(device_, &header_, sizeof(header_));
    waveOutClose(device_);
    device_ = nullptr;
    header_ = {};
    samples_.clear();
}

void AudioPlayer::collect() {
    if (device_ && (header_.dwFlags & WHDR_DONE)) stop();
}

bool AudioPlayer::play(Sound sound, int volume) {
    stop();
    if (volume == 0) return true;
    return playPcm(synthesize(sound, volume));
}

bool AudioPlayer::playPcm(std::vector<std::int16_t> samples) {
    stop();
    if (samples.empty()) return true;
    samples_ = std::move(samples);
    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = kSampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = kSampleRate * 2;
    if (waveOutOpen(&device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        device_ = nullptr;
        samples_.clear();
        return false;
    }
    header_.lpData = reinterpret_cast<LPSTR>(samples_.data());
    header_.dwBufferLength = static_cast<DWORD>(samples_.size() * sizeof(std::int16_t));
    if (waveOutPrepareHeader(device_, &header_, sizeof(header_)) != MMSYSERR_NOERROR
        || waveOutWrite(device_, &header_, sizeof(header_)) != MMSYSERR_NOERROR) {
        stop();
        return false;
    }
    return true;
}

} // namespace little_timer
