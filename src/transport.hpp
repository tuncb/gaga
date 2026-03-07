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
    bool loop_enabled = false;
    bool finished = false;
};

uint32_t compute_frames_per_row(uint32_t sample_rate, int bpm, int lpb);
void initialize_transport(TransportState& transport, uint32_t frames_per_row, bool loop_enabled, bool has_rows);
void apply_row_event(
    const PatternData& pattern,
    uint32_t row,
    SynthVoice& voice,
    std::span<const float> frequency_hz,
    uint32_t sample_rate);

}  // namespace gaga
