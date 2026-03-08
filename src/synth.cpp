#include "synth.hpp"

#include <algorithm>
#include <cmath>

namespace gaga {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kAmplitude = 0.15f;

float phase_fraction(float phase) {
    return phase / kTwoPi;
}

float square_wave(float phase) {
    return phase < (kTwoPi * 0.5f) ? 1.0f : -1.0f;
}

float saw_wave(float phase) {
    return 2.0f * phase_fraction(phase) - 1.0f;
}

float triangle_wave(float phase) {
    return 1.0f - 4.0f * std::abs(phase_fraction(phase) - 0.5f);
}

float next_noise_sample(SynthVoice& voice) {
    uint32_t state = voice.noise_state;
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    voice.noise_state = state == 0 ? 0x12345678U : state;

    const float normalized = static_cast<float>(voice.noise_state) / static_cast<float>(UINT32_MAX);
    return normalized * 2.0f - 1.0f;
}

}  // namespace

tl::expected<SynthType, std::string> parse_synth_type(std::string_view name) {
    if (name == "sine") {
        return SynthType::Sine;
    }
    if (name == "square") {
        return SynthType::Square;
    }
    if (name == "saw") {
        return SynthType::Saw;
    }
    if (name == "triangle") {
        return SynthType::Triangle;
    }
    if (name == "noise") {
        return SynthType::Noise;
    }

    return tl::unexpected(std::string("invalid synth type: ") + std::string(name));
}

std::string_view synth_type_name(SynthType type) {
    switch (type) {
    case SynthType::Sine:
        return "sine";
    case SynthType::Square:
        return "square";
    case SynthType::Saw:
        return "saw";
    case SynthType::Triangle:
        return "triangle";
    case SynthType::Noise:
        return "noise";
    }

    return "unknown";
}

void note_on(SynthVoice& voice, float frequency_hz, uint32_t sample_rate, SynthType type) {
    voice.active = true;
    voice.type = type;
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

    float waveform = 0.0f;
    switch (voice.type) {
    case SynthType::Sine:
        waveform = std::sin(voice.phase);
        break;
    case SynthType::Square:
        waveform = square_wave(voice.phase);
        break;
    case SynthType::Saw:
        waveform = saw_wave(voice.phase);
        break;
    case SynthType::Triangle:
        waveform = triangle_wave(voice.phase);
        break;
    case SynthType::Noise:
        waveform = next_noise_sample(voice);
        break;
    }

    const float sample = waveform * kAmplitude;
    voice.phase += voice.phase_step;
    if (voice.phase >= kTwoPi) {
        voice.phase -= kTwoPi * std::floor(voice.phase / kTwoPi);
    }
    return sample;
}

}  // namespace gaga
