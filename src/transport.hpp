#pragma once

#include <cstdint>
#include <span>

#include "pattern.hpp"
#include "synth.hpp"

namespace gaga {

struct TransportState {
    uint32_t current_row = 0;
    uint32_t frames_until_row = 0;
    uint32_t frames_per_row = 0;
    uint32_t sample_rate = 48000;
    int bpm = 120;
    int lpb = 4;
    bool loop_enabled = false;
    bool finished = false;
};

uint32_t compute_frames_per_row(uint32_t sample_rate, int bpm, int lpb);
void initialize_transport(TransportState& transport, uint32_t sample_rate, int bpm, int lpb, bool loop_enabled, bool has_rows);
void set_transport_tempo(TransportState& transport, int bpm);
void apply_row_event(
    const PatternData& pattern,
    uint32_t row,
    TransportState& transport,
    SynthVoice& voice,
    float& master_gain,
    SynthType synth_type,
    std::span<const float> frequency_hz,
    uint32_t sample_rate);

}  // namespace gaga
