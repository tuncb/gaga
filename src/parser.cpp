#include "parser.hpp"

#include "note.hpp"

namespace gaga {

namespace {

void append_row(PatternData& pattern, RowOp op, uint8_t note_index, uint32_t source_line) {
    pattern.op.push_back(op);
    pattern.note_index.push_back(note_index);
    pattern.source_line.push_back(source_line);
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

}  // namespace

ParseResult parse_pattern(const SourceText& source, const TokenStream& stream) {
    (void)source;

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

        if (kind == TokenKind::TripleDash) {
            row_op = RowOp::Empty;
            ++index;
        } else if (kind == TokenKind::Off) {
            row_op = RowOp::NoteOff;
            ++index;
        } else if (kind == TokenKind::NoteLetter) {
            if (index + 2 >= stream.size()) {
                result.diagnostics.push_back(make_diagnostic(stream, index, DiagnosticKind::UnexpectedToken));
                break;
            }

            if ((stream.kind[index + 1] != TokenKind::Sharp && stream.kind[index + 1] != TokenKind::Dash) ||
                stream.kind[index + 2] != TokenKind::Digit) {
                if (stream.kind[index + 1] == TokenKind::Invalid ||
                    (index + 2 < stream.size() && stream.kind[index + 2] == TokenKind::Invalid)) {
                    skip_to_line_end(stream, index);
                    continue;
                }
                const size_t bad_index =
                    stream.kind[index + 1] != TokenKind::Sharp && stream.kind[index + 1] != TokenKind::Dash
                        ? index + 1
                        : index + 2;
                result.diagnostics.push_back(make_diagnostic(stream, bad_index, DiagnosticKind::UnexpectedToken));
                skip_to_line_end(stream, index);
                continue;
            }

            const char letter = source.bytes[stream.offset[index]];
            const char accidental = source.bytes[stream.offset[index + 1]];
            const char octave = source.bytes[stream.offset[index + 2]];
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
            index += 3;
        } else {
            result.diagnostics.push_back(make_diagnostic(stream, index, DiagnosticKind::UnexpectedToken));
            skip_to_line_end(stream, index);
            continue;
        }

        if (stream.kind[index] == TokenKind::Comment) {
            ++index;
        } else if (!is_line_end(stream.kind[index])) {
            row_valid = false;
            result.diagnostics.push_back(make_diagnostic(stream, index, DiagnosticKind::TrailingTokens));
            skip_to_line_end(stream, index);
        }

        if (row_valid) {
            append_row(result.pattern, row_op, row_note_index, stream.line[row_start]);
        }

        if (index < stream.size() && stream.kind[index] == TokenKind::Newline) {
            ++index;
        }
    }

    return result;
}

}  // namespace gaga
