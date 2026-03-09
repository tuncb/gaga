#include "pattern.hpp"

#include <sstream>
#include <iomanip>

#include "note.hpp"

namespace gaga {

namespace {

std::string hex_byte_to_string(uint8_t value) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value);
    return out.str();
}

}  // namespace

std::string row_event_to_string(RowOp op, uint8_t note_index) {
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

std::string fx_command_to_string(FxCommand command) {
    switch (command) {
    case FxCommand::Volume:
        return "VOL";
    case FxCommand::Pitch:
        return "PIT";
    case FxCommand::Fine:
        return "FIN";
    case FxCommand::Transpose:
        return "TSP";
    case FxCommand::Tempo:
        return "TPO";
    case FxCommand::MasterVolume:
        return "VMV";
    }

    return "???";
}

std::string fx_to_string(FxCommand command, uint8_t value) {
    return fx_command_to_string(command) + " " + hex_byte_to_string(value);
}

std::string row_fx_to_string(const PatternData& pattern, size_t row) {
    if (row >= pattern.row_count() || pattern.fx_count[row] == 0) {
        return {};
    }

    std::string result;
    for (size_t fx_index = pattern.row_fx_begin(row); fx_index < pattern.row_fx_end(row); ++fx_index) {
        if (!result.empty()) {
            result += ' ';
        }
        result += fx_to_string(pattern.fx_command[fx_index], pattern.fx_value[fx_index]);
    }
    return result;
}

std::string row_to_string(const PatternData& pattern, size_t row) {
    if (row >= pattern.row_count()) {
        return "???";
    }

    std::string result = row_event_to_string(pattern.op[row], pattern.note_index[row]);
    const std::string fx = row_fx_to_string(pattern, row);
    if (!fx.empty()) {
        result += ' ';
        result += fx;
    }
    return result;
}

void print_normalized_rows(std::ostream& out, const PatternData& pattern) {
    for (size_t row = 0; row < pattern.row_count(); ++row) {
        out << std::setw(3) << std::setfill('0') << row << " " << row_to_string(pattern, row) << "\n";
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
