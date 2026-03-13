#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <tl/expected.hpp>

namespace gaga {

enum class SynthType : uint8_t {
    Sine,
    Square,
    Saw,
    Triangle,
    Noise
};

struct SynthVoice {
    bool active = false;
    bool has_selected_instrument = false;
    SynthType type = SynthType::Square;
    SynthType selected_type = SynthType::Square;
    float phase = 0.0f;
    float phase_step = 0.0f;
    float base_frequency_hz = 0.0f;
    float note_gain = 1.0f;
    float gain_scale = 1.0f;
    int8_t pitch_offset_semitones = 0;
    int8_t fine_offset = 0;
    int8_t transpose_semitones = 0;
    uint32_t noise_state = 0x12345678U;
};

tl::expected<SynthType, std::string> parse_synth_type(std::string_view name);
std::string_view synth_type_name(SynthType type);
tl::expected<uint8_t, std::string> parse_builtin_instrument_name(std::string_view name);
std::string_view builtin_instrument_name(uint8_t instrument);
void note_on(SynthVoice& voice, float frequency_hz, uint32_t sample_rate, SynthType type);
void change_note(SynthVoice& voice, float frequency_hz, uint32_t sample_rate);
void note_off(SynthVoice& voice);
void select_instrument(SynthVoice& voice, uint8_t instrument, SynthType fallback_type);
void set_note_volume(SynthVoice& voice, uint8_t value);
void set_volume_offset(SynthVoice& voice, uint8_t value);
void set_pitch_offset(SynthVoice& voice, uint8_t value, uint32_t sample_rate);
void set_fine_offset(SynthVoice& voice, uint8_t value, uint32_t sample_rate);
void set_transpose_offset(SynthVoice& voice, uint8_t value, uint32_t sample_rate);
float next_sample(SynthVoice& voice);

}  // namespace gaga
