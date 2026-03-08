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
    SynthType type = SynthType::Sine;
    float phase = 0.0f;
    float phase_step = 0.0f;
    uint32_t noise_state = 0x12345678U;
};

tl::expected<SynthType, std::string> parse_synth_type(std::string_view name);
std::string_view synth_type_name(SynthType type);
void note_on(SynthVoice& voice, float frequency_hz, uint32_t sample_rate, SynthType type);
void note_off(SynthVoice& voice);
float next_sample(SynthVoice& voice);

}  // namespace gaga
