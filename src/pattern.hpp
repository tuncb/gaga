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

struct PatternData {
    std::vector<RowOp> op;
    std::vector<uint8_t> note_index;
    std::vector<uint32_t> source_line;

    [[nodiscard]] size_t row_count() const { return op.size(); }
};

struct PatternSnapshot {
    PatternData pattern;
    std::vector<float> frequency_hz;
    std::vector<std::string> source_lines;
    uint64_t file_timestamp = 0;
    uint64_t file_size = 0;
    uint32_t display_generation = 0;
};

std::string row_to_string(RowOp op, uint8_t note_index);
void print_normalized_rows(std::ostream& out, const PatternData& pattern);
PatternSnapshot build_snapshot(
    PatternData pattern,
    std::vector<std::string> source_lines,
    uint64_t file_timestamp,
    uint64_t file_size,
    uint32_t display_generation);

}  // namespace gaga
