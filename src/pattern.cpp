#include "pattern.hpp"

#include <iomanip>

#include "note.hpp"

namespace gaga {

std::string row_to_string(RowOp op, uint8_t note_index) {
    switch (op) {
    case RowOp::Empty:
        return "---";
    case RowOp::NoteOn:
        return note_index_to_string(note_index);
    case RowOp::NoteOff:
        return "OFF";
    }

    return "???";
}

void print_normalized_rows(std::ostream& out, const PatternData& pattern) {
    for (size_t row = 0; row < pattern.row_count(); ++row) {
        out << std::setw(3) << std::setfill('0') << row << " "
            << row_to_string(pattern.op[row], pattern.note_index[row]) << "\n";
    }
    out << std::setfill(' ');
}

PatternSnapshot build_snapshot(
    PatternData pattern,
    std::vector<std::string> source_lines,
    uint64_t file_timestamp,
    uint64_t file_size,
    uint32_t display_generation) {
    PatternSnapshot snapshot;
    snapshot.pattern = std::move(pattern);
    snapshot.frequency_hz.resize(120);
    for (uint8_t note_index = 0; note_index < snapshot.frequency_hz.size(); ++note_index) {
        snapshot.frequency_hz[note_index] = note_index_to_frequency(note_index);
    }
    snapshot.source_lines = std::move(source_lines);
    snapshot.file_timestamp = file_timestamp;
    snapshot.file_size = file_size;
    snapshot.display_generation = display_generation;
    return snapshot;
}

}  // namespace gaga
