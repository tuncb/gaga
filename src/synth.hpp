#pragma once

#include <cstdint>

namespace gaga {

struct SynthVoice {
    bool active = false;
    float phase = 0.0f;
    float phase_step = 0.0f;
};

void note_on(SynthVoice& voice, float frequency_hz, uint32_t sample_rate);
void note_off(SynthVoice& voice);
float next_sample(SynthVoice& voice);

}  // namespace gaga
