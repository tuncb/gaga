#include "synth.hpp"

#include <algorithm>
#include <cmath>

namespace gaga {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kAmplitude = 0.15f;
constexpr float kMinFrequencyHz = 1.0f;
constexpr SynthType kInstrumentBank[] = {
    SynthType::Sine,
    SynthType::Square,
    SynthType::Saw,
    SynthType::Triangle,
    SynthType::Noise,
};

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

float clamped_frequency(float frequency_hz, uint32_t sample_rate) {
    const float max_frequency_hz = static_cast<float>(sample_rate) * 0.45f;
    return (std::clamp)(frequency_hz, kMinFrequencyHz, max_frequency_hz);
}

float effective_frequency_hz(const SynthVoice& voice, uint32_t sample_rate) {
    const float fine_semitones = static_cast<float>(voice.fine_offset) / 128.0f;
    const float semitone_offset =
        static_cast<float>(voice.transpose_semitones) +
        static_cast<float>(voice.pitch_offset_semitones) +
        fine_semitones;
    const float multiplier = std::pow(2.0f, semitone_offset / 12.0f);
    return clamped_frequency(voice.base_frequency_hz * multiplier, sample_rate);
}

void refresh_pitch_state(SynthVoice& voice, uint32_t sample_rate) {
    if (!voice.active || voice.base_frequency_hz <= 0.0f) {
        return;
    }

    voice.phase_step = kTwoPi * effective_frequency_hz(voice, sample_rate) / static_cast<float>(sample_rate);
}

float gain_scale_from_raw(uint8_t value) {
    const float signed_offset = static_cast<float>(static_cast<int8_t>(value));
    return (std::clamp)(1.0f + signed_offset / 64.0f, 0.0f, 4.0f);
}

SynthType synth_type_from_instrument(uint8_t instrument, SynthType fallback_type) {
    if (instrument < std::size(kInstrumentBank)) {
        return kInstrumentBank[instrument];
    }
    return fallback_type;
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

tl::expected<uint8_t, std::string> parse_builtin_instrument_name(std::string_view name) {
    const auto synth_type = parse_synth_type(name);
    if (!synth_type) {
        return tl::unexpected(synth_type.error());
    }

    switch (synth_type.value()) {
    case SynthType::Sine:
        return 0;
    case SynthType::Square:
        return 1;
    case SynthType::Saw:
        return 2;
    case SynthType::Triangle:
        return 3;
    case SynthType::Noise:
        return 4;
    }

    return tl::unexpected(std::string("invalid synth type: ") + std::string(name));
}

std::string_view builtin_instrument_name(uint8_t instrument) {
    if (instrument < std::size(kInstrumentBank)) {
        return synth_type_name(kInstrumentBank[instrument]);
    }

    return {};
}

void note_on(SynthVoice& voice, float frequency_hz, uint32_t sample_rate, SynthType type) {
    voice.active = true;
    voice.type = type;
    voice.selected_type = type;
    voice.phase = 0.0f;
    voice.base_frequency_hz = frequency_hz;
    voice.phase_step = 0.0f;
    refresh_pitch_state(voice, sample_rate);
}

void change_note(SynthVoice& voice, float frequency_hz, uint32_t sample_rate) {
    if (!voice.active) {
        note_on(voice, frequency_hz, sample_rate, voice.selected_type);
        return;
    }

    voice.base_frequency_hz = frequency_hz;
    refresh_pitch_state(voice, sample_rate);
}

void note_off(SynthVoice& voice) {
    voice.active = false;
    voice.phase = 0.0f;
    voice.phase_step = 0.0f;
}

void select_instrument(SynthVoice& voice, uint8_t instrument, SynthType fallback_type) {
    voice.has_selected_instrument = true;
    voice.selected_type = synth_type_from_instrument(instrument, fallback_type);
}

void set_note_volume(SynthVoice& voice, uint8_t value) {
    const uint8_t clamped = (std::min)(value, static_cast<uint8_t>(0x7F));
    voice.note_gain = static_cast<float>(clamped) / 127.0f;
}

void set_volume_offset(SynthVoice& voice, uint8_t value) {
    voice.gain_scale = gain_scale_from_raw(value);
}

void set_pitch_offset(SynthVoice& voice, uint8_t value, uint32_t sample_rate) {
    voice.pitch_offset_semitones = static_cast<int8_t>(value);
    refresh_pitch_state(voice, sample_rate);
}

void set_fine_offset(SynthVoice& voice, uint8_t value, uint32_t sample_rate) {
    voice.fine_offset = static_cast<int8_t>(value);
    refresh_pitch_state(voice, sample_rate);
}

void set_transpose_offset(SynthVoice& voice, uint8_t value, uint32_t sample_rate) {
    voice.transpose_semitones = static_cast<int8_t>(value);
    refresh_pitch_state(voice, sample_rate);
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

    const float sample = waveform * kAmplitude * voice.note_gain * voice.gain_scale;
    voice.phase += voice.phase_step;
    if (voice.phase >= kTwoPi) {
        voice.phase -= kTwoPi * std::floor(voice.phase / kTwoPi);
    }
    return sample;
}

}  // namespace gaga
