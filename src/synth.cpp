#include "synth.hpp"

#include <cmath>

namespace gaga {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kAmplitude = 0.15f;

}  // namespace

void note_on(SynthVoice& voice, float frequency_hz, uint32_t sample_rate) {
    voice.active = true;
    voice.phase = 0.0f;
    voice.phase_step = kTwoPi * frequency_hz / static_cast<float>(sample_rate);
}

void note_off(SynthVoice& voice) {
    voice.active = false;
    voice.phase = 0.0f;
    voice.phase_step = 0.0f;
}

float next_sample(SynthVoice& voice) {
    if (!voice.active) {
        return 0.0f;
    }

    const float sample = std::sin(voice.phase) * kAmplitude;
    voice.phase += voice.phase_step;
    if (voice.phase >= kTwoPi) {
        voice.phase -= kTwoPi;
    }
    return sample;
}

}  // namespace gaga
