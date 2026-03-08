#include "transport.hpp"

#include <algorithm>
#include <cmath>

namespace gaga {

uint32_t compute_frames_per_row(uint32_t sample_rate, int bpm, int lpb) {
    const double seconds_per_row = 60.0 / (static_cast<double>(bpm) * static_cast<double>(lpb));
    return std::max<uint32_t>(1, static_cast<uint32_t>(std::llround(seconds_per_row * sample_rate)));
}

void initialize_transport(TransportState& transport, uint32_t frames_per_row, bool loop_enabled, bool has_rows) {
    transport.current_row = 0;
    transport.frames_until_row = frames_per_row;
    transport.frames_per_row = frames_per_row;
    transport.loop_enabled = loop_enabled;
    transport.finished = !has_rows;
}

void apply_row_event(
    const PatternData& pattern,
    uint32_t row,
    SynthVoice& voice,
    SynthType synth_type,
    std::span<const float> frequency_hz,
    uint32_t sample_rate) {
    if (row >= pattern.row_count()) {
        note_off(voice);
        return;
    }

    switch (pattern.op[row]) {
    case RowOp::Empty:
        break;
    case RowOp::NoteOn:
        note_on(voice, frequency_hz[pattern.note_index[row]], sample_rate, synth_type);
        break;
    case RowOp::NoteOff:
        note_off(voice);
        break;
    }
}

}  // namespace gaga
