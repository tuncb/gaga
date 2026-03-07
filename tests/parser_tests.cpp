#include <iostream>

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

bool test_invalid_note_shape() {
    gaga::SourceText source{"memory", std::vector<char>{'C', '4', '\n'}};

    const auto tokenization = gaga::tokenize(source);
    const auto parse = gaga::parse_pattern(source, tokenization.stream);
    if (!tokenization.diagnostics.empty()) {
        std::cerr << "unexpected tokenizer diagnostics for C4\n";
        return false;
    }

    if (parse.diagnostics.size() != 1) {
        std::cerr << "expected one parser diagnostic\n";
        return false;
    }

    return parse.diagnostics[0].kind == gaga::DiagnosticKind::UnexpectedToken;
}

}  // namespace

int main() {
    if (!test_comments_and_rows()) {
        return 1;
    }

    if (!test_invalid_note_shape()) {
        return 1;
    }

    return 0;
}
