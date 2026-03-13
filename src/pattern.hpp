#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace gaga {

enum class RowOp : uint8_t {
    Empty,
    NoteOn,
    NoteOff
};

enum class FxCommand : uint8_t {
    Volume,
    Pitch,
    Fine,
    Transpose,
    Tempo,
    MasterVolume
};

constexpr uint8_t kRowColumnVolume = 1U << 0U;
constexpr uint8_t kRowColumnInstrument = 1U << 1U;

struct PatternData {
    std::vector<RowOp> op;
    std::vector<uint8_t> midi_note;
    std::vector<uint32_t> source_line;
    std::vector<uint8_t> row_columns;
    std::vector<uint8_t> volume;
    std::vector<uint8_t> instrument;
    std::vector<uint32_t> fx_start;
    std::vector<uint16_t> fx_count;
    std::vector<FxCommand> fx_command;
    std::vector<uint8_t> fx_value;

    [[nodiscard]] size_t row_count() const { return op.size(); }
    [[nodiscard]] bool row_has_volume(size_t row) const { return (row_columns[row] & kRowColumnVolume) != 0; }
    [[nodiscard]] bool row_has_instrument(size_t row) const {
        return (row_columns[row] & kRowColumnInstrument) != 0;
    }
    [[nodiscard]] size_t row_fx_begin(size_t row) const { return fx_start[row]; }
    [[nodiscard]] size_t row_fx_end(size_t row) const { return fx_start[row] + fx_count[row]; }
};

struct PatternSnapshot {
    PatternData pattern;
    std::vector<float> frequency_hz;
    std::vector<std::string> source_lines;
    uint64_t file_timestamp = 0;
    uint64_t file_size = 0;
    uint32_t display_generation = 0;
};

std::string row_event_to_string(RowOp op, uint8_t midi_note);
std::string row_columns_to_string(const PatternData& pattern, size_t row);
std::string fx_command_to_string(FxCommand command);
std::string fx_to_string(FxCommand command, uint8_t value);
std::string row_fx_to_string(const PatternData& pattern, size_t row);
std::string row_to_string(const PatternData& pattern, size_t row);
void print_normalized_rows(std::ostream& out, const PatternData& pattern);
PatternSnapshot build_snapshot(
    PatternData pattern,
    std::vector<std::string> source_lines,
    uint64_t file_timestamp,
    uint64_t file_size,
    uint32_t display_generation);

}  // namespace gaga
