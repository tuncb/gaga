#include "parser.hpp"

#include <string_view>

#include "note.hpp"

namespace gaga {

namespace {

void append_row(
    PatternData& pattern,
    RowOp op,
    uint8_t note_index,
    uint32_t source_line,
    const std::vector<FxCommand>& row_fx_command,
    const std::vector<uint8_t>& row_fx_value) {
    pattern.op.push_back(op);
    pattern.note_index.push_back(note_index);
    pattern.source_line.push_back(source_line);
    pattern.fx_start.push_back(static_cast<uint32_t>(pattern.fx_command.size()));
    pattern.fx_count.push_back(static_cast<uint16_t>(row_fx_command.size()));
    pattern.fx_command.insert(pattern.fx_command.end(), row_fx_command.begin(), row_fx_command.end());
    pattern.fx_value.insert(pattern.fx_value.end(), row_fx_value.begin(), row_fx_value.end());
}

Diagnostic make_diagnostic(const TokenStream& stream, size_t index, DiagnosticKind kind) {
    return Diagnostic{
        kind,
        stream.line[index],
        stream.column[index],
        stream.offset[index],
        stream.length[index],
    };
}

void skip_to_line_end(const TokenStream& stream, size_t& index) {
    while (index < stream.size() &&
           stream.kind[index] != TokenKind::Newline &&
           stream.kind[index] != TokenKind::EndOfFile) {
        ++index;
    }
}

bool is_line_end(TokenKind kind) {
    return kind == TokenKind::Newline || kind == TokenKind::EndOfFile;
}

bool token_equals(const SourceText& source, const TokenStream& stream, size_t index, std::string_view text) {
    return token_text(source, stream, index) == text;
}

tl::expected<FxCommand, DiagnosticKind> parse_fx_command(
    const SourceText& source,
    const TokenStream& stream,
    size_t index) {
    const auto text = token_text(source, stream, index);
    if (text == "VOL") {
        return FxCommand::Volume;
    }
    if (text == "PIT") {
        return FxCommand::Pitch;
    }
    if (text == "FIN") {
        return FxCommand::Fine;
    }

    return tl::unexpected(DiagnosticKind::InvalidToken);
}

tl::expected<uint8_t, DiagnosticKind> parse_hex_byte(
    const SourceText& source,
    const TokenStream& stream,
    size_t index) {
    const auto text = token_text(source, stream, index);
    if (text.size() != 2) {
        return tl::unexpected(DiagnosticKind::InvalidToken);
    }

    auto hex_digit = [](char value) -> int {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'A' && value <= 'F') {
            return 10 + (value - 'A');
        }
        return -1;
    };

    const int high = hex_digit(text[0]);
    const int low = hex_digit(text[1]);
    if (high < 0 || low < 0) {
        return tl::unexpected(DiagnosticKind::InvalidToken);
    }

    return static_cast<uint8_t>((high << 4U) | low);
}

}  // namespace

ParseResult parse_pattern(const SourceText& source, const TokenStream& stream) {
    ParseResult result;
    size_t index = 0;

    while (index < stream.size()) {
        const TokenKind kind = stream.kind[index];

        if (kind == TokenKind::Newline) {
            ++index;
            continue;
        }

        if (kind == TokenKind::EndOfFile) {
            break;
        }

        if (kind == TokenKind::Comment) {
            ++index;
            if (index < stream.size() && stream.kind[index] == TokenKind::Newline) {
                ++index;
            }
            continue;
        }

        if (kind == TokenKind::Invalid) {
            skip_to_line_end(stream, index);
            continue;
        }

        const size_t row_start = index;
        bool row_valid = true;
        RowOp row_op = RowOp::Empty;
        uint8_t row_note_index = 0;
        std::vector<FxCommand> row_fx_command;
        std::vector<uint8_t> row_fx_value;

        if (kind == TokenKind::TripleDash) {
            row_op = RowOp::Empty;
            ++index;
        } else if (kind == TokenKind::Word && token_equals(source, stream, index, "OFF")) {
            row_op = RowOp::NoteOff;
            ++index;
        } else if (kind == TokenKind::Note) {
            const auto text = token_text(source, stream, index);
            const char letter = text[0];
            const char accidental = text[1];
            const char octave = text[2];
            const auto parsed_note = decode_note(letter, accidental, octave);
            if (!parsed_note) {
                result.diagnostics.push_back(make_diagnostic(
                    stream,
                    row_start,
                    parsed_note.error() == NoteParseError::NoteOutOfRange ? DiagnosticKind::NoteOutOfRange
                                                                          : DiagnosticKind::InvalidToken));
                skip_to_line_end(stream, index);
                continue;
            }

            row_op = RowOp::NoteOn;
            row_note_index = parsed_note.value().note_index;
            ++index;
        } else {
            result.diagnostics.push_back(make_diagnostic(stream, index, DiagnosticKind::UnexpectedToken));
            skip_to_line_end(stream, index);
            continue;
        }

        while (index < stream.size() && !is_line_end(stream.kind[index]) && stream.kind[index] != TokenKind::Comment) {
            if (stream.kind[index] == TokenKind::Invalid) {
                row_valid = false;
                skip_to_line_end(stream, index);
                break;
            }

            if (stream.kind[index] != TokenKind::Word) {
                row_valid = false;
                result.diagnostics.push_back(make_diagnostic(stream, index, DiagnosticKind::TrailingTokens));
                skip_to_line_end(stream, index);
                break;
            }

            const auto fx_command = parse_fx_command(source, stream, index);
            if (!fx_command) {
                row_valid = false;
                result.diagnostics.push_back(make_diagnostic(stream, index, fx_command.error()));
                skip_to_line_end(stream, index);
                break;
            }
            ++index;

            if (index >= stream.size() || stream.kind[index] != TokenKind::HexByte) {
                row_valid = false;
                const size_t diagnostic_index = index < stream.size() ? index : index - 1;
                result.diagnostics.push_back(make_diagnostic(stream, diagnostic_index, DiagnosticKind::UnexpectedToken));
                skip_to_line_end(stream, index);
                break;
            }

            const auto fx_value = parse_hex_byte(source, stream, index);
            if (!fx_value) {
                row_valid = false;
                result.diagnostics.push_back(make_diagnostic(stream, index, fx_value.error()));
                skip_to_line_end(stream, index);
                break;
            }

            row_fx_command.push_back(fx_command.value());
            row_fx_value.push_back(fx_value.value());
            ++index;
        }

        if (row_valid && index < stream.size() && stream.kind[index] == TokenKind::Comment) {
            ++index;
        }

        if (row_valid) {
            append_row(result.pattern, row_op, row_note_index, stream.line[row_start], row_fx_command, row_fx_value);
        }

        if (index < stream.size() && stream.kind[index] == TokenKind::Newline) {
            ++index;
        }
    }

    return result;
}

}  // namespace gaga
