#include <iostream>

#include "pattern.hpp"
#include "parser.hpp"
#include "tokenizer.hpp"

namespace {

bool test_comments_and_rows() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{'#', ' ', 'x', '\n', 'C', '4', '\n', '-', '-', '-', '\n', 'O', 'F', 'F'}};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (!parse.diagnostics.empty()) {
        std::cerr << "unexpected parser diagnostics\n";
        return false;
    }

    if (parse.pattern.row_count() != 3) {
        std::cerr << "expected 3 rows\n";
        return false;
    }

    return parse.pattern.op[0] == gaga::RowOp::NoteOn &&
           parse.pattern.op[1] == gaga::RowOp::Empty &&
           parse.pattern.op[2] == gaga::RowOp::NoteOff;
}

bool test_fx_rows_parse() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{
            'C', '4', ' ', 'V', 'O', 'L', ' ', '2', '0', ' ', 'P', 'I', 'T', ' ', '0', '1', ' ', 'T', 'S', 'P', ' ', 'F', 'F', '\n',
            '-', '-', '-', ' ', 'F', 'I', 'N', ' ', 'F', '0', ' ', 'T', 'P', 'O', ' ', '9', '0', '\n',
            'O', 'F', 'F', ' ', 'V', 'M', 'V', ' ', 'C', '0', ' ', '#', ' ', 'x',
        }};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for fx rows\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (!parse.diagnostics.empty()) {
        std::cerr << "unexpected parser diagnostics for fx rows\n";
        return false;
    }

    if (parse.pattern.row_count() != 3) {
        std::cerr << "expected 3 parsed rows with fx\n";
        return false;
    }

    if (parse.pattern.fx_count.size() != 3 ||
        parse.pattern.fx_count[0] != 3 ||
        parse.pattern.fx_count[1] != 2 ||
        parse.pattern.fx_count[2] != 1) {
        std::cerr << "unexpected fx counts\n";
        return false;
    }

    return parse.pattern.fx_command[0] == gaga::FxCommand::Volume &&
           parse.pattern.fx_value[0] == 0x20 &&
           parse.pattern.fx_command[1] == gaga::FxCommand::Pitch &&
           parse.pattern.fx_value[1] == 0x01 &&
           parse.pattern.fx_command[2] == gaga::FxCommand::Transpose &&
           parse.pattern.fx_value[2] == 0xFF &&
           parse.pattern.fx_command[3] == gaga::FxCommand::Fine &&
           parse.pattern.fx_value[3] == 0xF0 &&
           parse.pattern.fx_command[4] == gaga::FxCommand::Tempo &&
           parse.pattern.fx_value[4] == 0x90 &&
           parse.pattern.fx_command[5] == gaga::FxCommand::MasterVolume &&
           parse.pattern.fx_value[5] == 0xC0;
}

bool test_mixed_case_pattern_tokens_parse() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{
            'c', '4', ' ', 'v', 'O', 'l', ' ', '2', 'a', ' ', 'p', 'I', 't', ' ', '0', '1', ' ', 't', 'S', 'p', ' ', 'f', 'F', '\n',
            '-', '-', '-', ' ', 'F', 'i', 'N', ' ', 'f', '0', ' ', 'T', 'p', 'O', ' ', '9', '0', '\n',
            'o', 'F', 'f', ' ', 'V', 'm', 'V', ' ', 'c', '0', ' ', '#', ' ', 'x',
        }};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for mixed-case pattern tokens\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (!parse.diagnostics.empty()) {
        std::cerr << "unexpected parser diagnostics for mixed-case pattern tokens\n";
        return false;
    }

    if (parse.pattern.row_count() != 3) {
        std::cerr << "expected 3 parsed rows for mixed-case pattern tokens\n";
        return false;
    }

    if (parse.pattern.op[0] != gaga::RowOp::NoteOn ||
        parse.pattern.midi_note[0] != 60 ||
        parse.pattern.op[1] != gaga::RowOp::Empty ||
        parse.pattern.op[2] != gaga::RowOp::NoteOff) {
        std::cerr << "unexpected row events for mixed-case pattern tokens\n";
        return false;
    }

    if (parse.pattern.fx_count.size() != 3 ||
        parse.pattern.fx_count[0] != 3 ||
        parse.pattern.fx_count[1] != 2 ||
        parse.pattern.fx_count[2] != 1) {
        std::cerr << "unexpected mixed-case fx counts\n";
        return false;
    }

    return parse.pattern.fx_command[0] == gaga::FxCommand::Volume &&
           parse.pattern.fx_value[0] == 0x2A &&
           parse.pattern.fx_command[1] == gaga::FxCommand::Pitch &&
           parse.pattern.fx_value[1] == 0x01 &&
           parse.pattern.fx_command[2] == gaga::FxCommand::Transpose &&
           parse.pattern.fx_value[2] == 0xFF &&
           parse.pattern.fx_command[3] == gaga::FxCommand::Fine &&
           parse.pattern.fx_value[3] == 0xF0 &&
           parse.pattern.fx_command[4] == gaga::FxCommand::Tempo &&
           parse.pattern.fx_value[4] == 0x90 &&
           parse.pattern.fx_command[5] == gaga::FxCommand::MasterVolume &&
           parse.pattern.fx_value[5] == 0xC0;
}

bool test_volume_and_instrument_columns_parse() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{
            'C', '4', ' ', '6', '4', ' ', 's', 'q', 'u', 'a', 'r', 'e', ' ', 'V', 'O', 'L', ' ', '2', '0', '\n',
            'D', '4', ' ', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', '\n',
            'E', '4', ' ', '4', '0', '\n',
            '-', '-', '-', ' ', 'n', 'o', 'i', 's', 'e',
        }};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for volume/instrument columns\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (!parse.diagnostics.empty()) {
        std::cerr << "unexpected parser diagnostics for volume/instrument columns\n";
        return false;
    }

    if (parse.pattern.row_count() != 4) {
        std::cerr << "expected 4 parsed rows for volume/instrument columns\n";
        return false;
    }

    if (parse.pattern.row_columns[0] != (gaga::kRowColumnVolume | gaga::kRowColumnInstrument) ||
        parse.pattern.volume[0] != 0x64 ||
        parse.pattern.instrument[0] != 0x01) {
        std::cerr << "unexpected parsed columns on row 0\n";
        return false;
    }

    if (parse.pattern.row_columns[1] != gaga::kRowColumnInstrument ||
        parse.pattern.instrument[1] != 0x03 ||
        parse.pattern.row_has_volume(1)) {
        std::cerr << "unexpected parsed columns on row 1\n";
        return false;
    }

    if (parse.pattern.row_columns[2] != gaga::kRowColumnVolume ||
        parse.pattern.volume[2] != 0x40 ||
        parse.pattern.row_has_instrument(2)) {
        std::cerr << "unexpected parsed columns on row 2\n";
        return false;
    }

    return parse.pattern.row_columns[3] == gaga::kRowColumnInstrument &&
           parse.pattern.instrument[3] == 0x04 &&
           gaga::row_to_string(parse.pattern, 0) == "C4 64 square VOL 20" &&
           gaga::row_to_string(parse.pattern, 1) == "D4 triangle" &&
           gaga::row_to_string(parse.pattern, 2) == "E4 40" &&
           gaga::row_to_string(parse.pattern, 3) == "--- noise";
}

bool test_named_instrument_columns_parse() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{
            'C', '4', ' ', '6', '4', ' ', 's', 'a', 'w', '\n',
            'D', '4', ' ', '-', '-', ' ', 't', 'r', 'i', 'a', 'n', 'g', 'l', 'e', '\n',
            'E', '4', ' ', '-', '-', ' ', 'n', 'o', 'i', 's', 'e',
        }};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for named instrument columns\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (!parse.diagnostics.empty()) {
        std::cerr << "unexpected parser diagnostics for named instrument columns\n";
        return false;
    }

    return parse.pattern.row_columns[0] == (gaga::kRowColumnVolume | gaga::kRowColumnInstrument) &&
           parse.pattern.volume[0] == 0x64 &&
           parse.pattern.instrument[0] == 0x02 &&
           gaga::row_to_string(parse.pattern, 0) == "C4 64 saw" &&
           parse.pattern.row_columns[1] == gaga::kRowColumnInstrument &&
           parse.pattern.instrument[1] == 0x03 &&
           parse.pattern.row_columns[2] == gaga::kRowColumnInstrument &&
           parse.pattern.instrument[2] == 0x04 &&
           gaga::row_to_string(parse.pattern, 1) == "D4 triangle" &&
           gaga::row_to_string(parse.pattern, 2) == "E4 noise";
}

bool test_column_values_over_7f_rejected() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{'C', '4', ' ', '8', '0', ' ', '0', '1', '\n'}};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for invalid column value\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (parse.diagnostics.size() != 1) {
        std::cerr << "expected parser diagnostic for invalid column value\n";
        return false;
    }

    return parse.diagnostics[0].kind == gaga::DiagnosticKind::InvalidToken;
}

bool test_unknown_named_instrument_rejected() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{'C', '4', ' ', '-', '-', ' ', 's', 'u', 'p', 'e', 'r', 's', 'a', 'w', '\n'}};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for unknown named instrument\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (parse.diagnostics.size() != 1) {
        std::cerr << "expected parser diagnostic for unknown named instrument\n";
        return false;
    }

    return parse.diagnostics[0].kind == gaga::DiagnosticKind::InvalidToken;
}

bool test_numeric_instrument_rejected() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{'C', '4', ' ', '6', '4', ' ', '0', '1', '\n'}};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for numeric instrument\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (parse.diagnostics.size() != 1) {
        std::cerr << "expected parser diagnostic for numeric instrument\n";
        return false;
    }

    return parse.diagnostics[0].kind == gaga::DiagnosticKind::InvalidToken;
}

bool test_scientific_pitch_range_limits() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{
            'C', '-', '1', '\n',
            'G', '9', '\n',
            'G', '#', '9', '\n',
        }};

    const auto tokenization = gaga::tokenize(source);
    const auto parse = gaga::parse_pattern(source, tokenization.stream);

    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for SPN range limits\n";
        return false;
    }

    if (parse.diagnostics.size() != 1) {
        std::cerr << "expected one parser diagnostic for out-of-range SPN\n";
        return false;
    }

    return parse.pattern.row_count() == 2 &&
           parse.pattern.midi_note[0] == 0 &&
           parse.pattern.midi_note[1] == 127 &&
           parse.diagnostics[0].kind == gaga::DiagnosticKind::NoteOutOfRange;
}

bool test_unknown_fx_command_rejected() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{'C', '4', ' ', 'P', 'A', 'N', ' ', '1', '0', '\n'}};

    const auto tokenization = gaga::tokenize(source);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for unknown fx command\n";
        return false;
    }

    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (parse.diagnostics.size() != 1) {
        std::cerr << "expected parser diagnostic for unknown fx command\n";
        return false;
    }

    return parse.diagnostics[0].kind == gaga::DiagnosticKind::InvalidToken;
}

}  // namespace

int main() {
    if (!test_comments_and_rows()) {
        return 1;
    }

    if (!test_fx_rows_parse()) {
        return 1;
    }

    if (!test_mixed_case_pattern_tokens_parse()) {
        return 1;
    }

    if (!test_volume_and_instrument_columns_parse()) {
        return 1;
    }

    if (!test_named_instrument_columns_parse()) {
        return 1;
    }

    if (!test_column_values_over_7f_rejected()) {
        return 1;
    }

    if (!test_unknown_named_instrument_rejected()) {
        return 1;
    }

    if (!test_numeric_instrument_rejected()) {
        return 1;
    }

    if (!test_scientific_pitch_range_limits()) {
        return 1;
    }

    if (!test_unknown_fx_command_rejected()) {
        return 1;
    }

    return 0;
}
