#include <iostream>

#include "pattern.hpp"
#include "parser.hpp"
#include "tokenizer.hpp"

namespace {

bool test_comments_and_rows() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{'#', ' ', 'x', '\n', 'C', '-', '4', '\n', '-', '-', '-', '\n', 'O', 'F', 'F'}};

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
            'C', '-', '4', ' ', 'V', 'O', 'L', ' ', '2', '0', ' ', 'P', 'I', 'T', ' ', '0', '1', ' ', 'T', 'S', 'P', ' ', 'F', 'F', '\n',
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

bool test_invalid_note_shape() {
    gaga::SourceText source{"memory", std::vector<char>{'C', '4', '\n'}};

    const auto tokenization = gaga::tokenize(source);
    const auto parse = gaga::parse_pattern(source, tokenization.stream);

    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for C4\n";
        return false;
    }

    if (parse.diagnostics.size() != 1) {
        std::cerr << "expected one parser diagnostic for C4\n";
        return false;
    }

    return parse.diagnostics[0].kind == gaga::DiagnosticKind::UnexpectedToken;
}

bool test_unknown_fx_command_rejected() {
    gaga::SourceText source{
        "memory",
        std::vector<char>{'C', '-', '4', ' ', 'P', 'A', 'N', ' ', '1', '0', '\n'}};

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

    if (!test_invalid_note_shape()) {
        return 1;
    }

    if (!test_unknown_fx_command_rejected()) {
        return 1;
    }

    return 0;
}
