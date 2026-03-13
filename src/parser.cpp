#include "parser.hpp"

#include <string_view>
#include <utility>

#include "note.hpp"
#include "synth.hpp"

namespace gaga {

namespace {

char ascii_upper(char value) {
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - 'a' + 'A');
    }
    return value;
}

bool ascii_iequals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (size_t index = 0; index < left.size(); ++index) {
        if (ascii_upper(left[index]) != ascii_upper(right[index])) {
            return false;
        }
    }

    return true;
}

void append_row(
    PatternData& pattern,
    RowOp op,
    uint8_t midi_note,
    uint32_t source_line,
    uint8_t row_columns,
    uint8_t volume,
    uint8_t instrument,
    const std::vector<FxCommand>& row_fx_command,
    const std::vector<uint8_t>& row_fx_value) {
    pattern.op.push_back(op);
    pattern.midi_note.push_back(midi_note);
    pattern.source_line.push_back(source_line);
    pattern.row_columns.push_back(row_columns);
    pattern.volume.push_back(volume);
    pattern.instrument.push_back(instrument);
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

bool starts_like_note(std::string_view text) {
    if (text.empty()) {
        return false;
    }

    const char normalized = ascii_upper(text[0]);
    return normalized >= 'A' && normalized <= 'G';
}

bool is_instrument_name_token(const SourceText& source, const TokenStream& stream, size_t index) {
    if (index >= stream.size() || stream.kind[index] != TokenKind::Atom) {
        return false;
    }

    return parse_builtin_instrument_name(token_text(source, stream, index)).has_value();
}

bool token_equals(const SourceText& source, const TokenStream& stream, size_t index, std::string_view text) {
    return ascii_iequals(token_text(source, stream, index), text);
}

tl::expected<FxCommand, DiagnosticKind> parse_fx_command(
    const SourceText& source,
    const TokenStream& stream,
    size_t index) {
    if (index >= stream.size() || stream.kind[index] != TokenKind::Atom) {
        return tl::unexpected(DiagnosticKind::InvalidToken);
    }

    const auto text = token_text(source, stream, index);
    if (ascii_iequals(text, "VOL")) {
        return FxCommand::Volume;
    }
    if (ascii_iequals(text, "PIT")) {
        return FxCommand::Pitch;
    }
    if (ascii_iequals(text, "FIN")) {
        return FxCommand::Fine;
    }
    if (ascii_iequals(text, "TSP")) {
        return FxCommand::Transpose;
    }
    if (ascii_iequals(text, "TPO")) {
        return FxCommand::Tempo;
    }
    if (ascii_iequals(text, "VMV")) {
        return FxCommand::MasterVolume;
    }

    return tl::unexpected(DiagnosticKind::InvalidToken);
}

tl::expected<uint8_t, DiagnosticKind> parse_hex_byte(
    const SourceText& source,
    const TokenStream& stream,
    size_t index) {
    if (index >= stream.size() || stream.kind[index] != TokenKind::Atom) {
        return tl::unexpected(DiagnosticKind::InvalidToken);
    }

    const auto text = token_text(source, stream, index);
    if (text.size() != 2) {
        return tl::unexpected(DiagnosticKind::InvalidToken);
    }

    auto hex_digit = [](char value) -> int {
        const char normalized = ascii_upper(value);
        if (normalized >= '0' && normalized <= '9') {
            return normalized - '0';
        }
        if (normalized >= 'A' && normalized <= 'F') {
            return 10 + (normalized - 'A');
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

tl::expected<std::pair<bool, uint8_t>, DiagnosticKind> parse_column_value(
    const SourceText& source,
    const TokenStream& stream,
    size_t index) {
    if (stream.kind[index] == TokenKind::DoubleDash) {
        return std::pair<bool, uint8_t>{false, 0};
    }

    const auto value = parse_hex_byte(source, stream, index);
    if (!value) {
        return tl::unexpected(value.error());
    }

    if (value.value() > 0x7F) {
        return tl::unexpected(DiagnosticKind::InvalidToken);
    }

    return std::pair<bool, uint8_t>{true, value.value()};
}

bool is_hex_byte_token(const SourceText& source, const TokenStream& stream, size_t index) {
    if (index >= stream.size() || stream.kind[index] != TokenKind::Atom) {
        return false;
    }

    return parse_hex_byte(source, stream, index).has_value();
}

tl::expected<std::pair<bool, uint8_t>, DiagnosticKind> parse_instrument_column_value(
    const SourceText& source,
    const TokenStream& stream,
    size_t index) {
    if (stream.kind[index] == TokenKind::DoubleDash) {
        return std::pair<bool, uint8_t>{false, 0};
    }

    const auto instrument = parse_builtin_instrument_name(token_text(source, stream, index));
    if (!instrument) {
        return tl::unexpected(DiagnosticKind::InvalidToken);
    }
    return std::pair<bool, uint8_t>{true, instrument.value()};
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

        const size_t row_start = index;
        bool row_valid = true;
        RowOp row_op = RowOp::Empty;
        uint8_t row_midi_note = 0;
        uint8_t row_columns = 0;
        uint8_t row_volume = 0;
        uint8_t row_instrument = 0;
        std::vector<FxCommand> row_fx_command;
        std::vector<uint8_t> row_fx_value;

        if (kind == TokenKind::TripleDash) {
            row_op = RowOp::Empty;
            ++index;
        } else if (kind == TokenKind::Atom && token_equals(source, stream, index, "OFF")) {
            row_op = RowOp::NoteOff;
            ++index;
        } else if (kind == TokenKind::Atom) {
            const auto parsed_note = parse_scientific_pitch(token_text(source, stream, index));
            if (!parsed_note) {
                const DiagnosticKind diagnostic_kind =
                    starts_like_note(token_text(source, stream, index))
                        ? (parsed_note.error() == NoteParseError::NoteOutOfRange ? DiagnosticKind::NoteOutOfRange
                                                                                 : DiagnosticKind::InvalidToken)
                        : DiagnosticKind::UnexpectedToken;
                result.diagnostics.push_back(make_diagnostic(
                    stream,
                    row_start,
                    diagnostic_kind));
                skip_to_line_end(stream, index);
                continue;
            }

            row_op = RowOp::NoteOn;
            row_midi_note = parsed_note.value().midi_note;
            ++index;
        } else {
            result.diagnostics.push_back(make_diagnostic(stream, index, DiagnosticKind::UnexpectedToken));
            skip_to_line_end(stream, index);
            continue;
        }

        if (index < stream.size() &&
            (stream.kind[index] == TokenKind::DoubleDash || is_hex_byte_token(source, stream, index))) {
            const auto volume = parse_column_value(source, stream, index);
            if (!volume) {
                result.diagnostics.push_back(make_diagnostic(stream, index, volume.error()));
                skip_to_line_end(stream, index);
                continue;
            }

            if (volume.value().first) {
                row_columns |= kRowColumnVolume;
                row_volume = volume.value().second;
            }
            ++index;

            if (index < stream.size() &&
                (stream.kind[index] == TokenKind::DoubleDash || is_instrument_name_token(source, stream, index))) {
                const auto instrument = parse_instrument_column_value(source, stream, index);
                if (!instrument) {
                    result.diagnostics.push_back(make_diagnostic(stream, index, instrument.error()));
                    skip_to_line_end(stream, index);
                    continue;
                }

                if (instrument.value().first) {
                    row_columns |= kRowColumnInstrument;
                    row_instrument = instrument.value().second;
                }
                ++index;
            }
        } else if (is_instrument_name_token(source, stream, index)) {
            const auto instrument = parse_instrument_column_value(source, stream, index);
            if (!instrument) {
                result.diagnostics.push_back(make_diagnostic(stream, index, instrument.error()));
                skip_to_line_end(stream, index);
                continue;
            }

            row_columns |= kRowColumnInstrument;
            row_instrument = instrument.value().second;
            ++index;
        }

        while (index < stream.size() && !is_line_end(stream.kind[index]) && stream.kind[index] != TokenKind::Comment) {
            if (stream.kind[index] != TokenKind::Atom) {
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

            if (index >= stream.size() || stream.kind[index] != TokenKind::Atom) {
                row_valid = false;
                const size_t diagnostic_index = index < stream.size() ? index : index - 1;
                result.diagnostics.push_back(make_diagnostic(stream, diagnostic_index, DiagnosticKind::UnexpectedToken));
                skip_to_line_end(stream, index);
                break;
            }

            const auto fx_value = parse_hex_byte(source, stream, index);
            if (!fx_value) {
                row_valid = false;
                result.diagnostics.push_back(make_diagnostic(stream, index, DiagnosticKind::UnexpectedToken));
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
            append_row(
                result.pattern,
                row_op,
                row_midi_note,
                stream.line[row_start],
                row_columns,
                row_volume,
                row_instrument,
                row_fx_command,
                row_fx_value);
        }

        if (index < stream.size() && stream.kind[index] == TokenKind::Newline) {
            ++index;
        }
    }

    return result;
}

}  // namespace gaga
