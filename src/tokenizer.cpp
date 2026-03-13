#include "tokenizer.hpp"

#include <cctype>

namespace gaga {

namespace {

char ascii_upper(char value) {
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - 'a' + 'A');
    }
    return value;
}

void push_token(
    TokenStream& stream,
    TokenKind kind,
    uint32_t offset,
    uint16_t length,
    uint32_t line,
    uint16_t column) {
    stream.kind.push_back(kind);
    stream.offset.push_back(offset);
    stream.length.push_back(length);
    stream.line.push_back(line);
    stream.column.push_back(column);
}

bool is_note_letter(char value) {
    const char normalized = ascii_upper(value);
    return normalized >= 'A' && normalized <= 'G';
}

bool is_word_letter(char value) {
    const char normalized = ascii_upper(value);
    return normalized >= 'A' && normalized <= 'Z';
}

bool is_hex_digit(char value) {
    const char normalized = ascii_upper(value);
    return (normalized >= '0' && normalized <= '9') || (normalized >= 'A' && normalized <= 'F');
}

bool is_token_boundary(char value) {
    return value == '\0' || value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '#';
}

void push_invalid_token(
    TokenizationResult& result,
    uint32_t offset,
    uint16_t length,
    uint32_t line,
    uint16_t column,
    DiagnosticKind diagnostic_kind) {
    push_token(result.stream, TokenKind::Invalid, offset, length, line, column);
    result.diagnostics.push_back(Diagnostic{diagnostic_kind, line, column, offset, length});
}

}  // namespace

TokenizationResult tokenize(const SourceText& source) {
    TokenizationResult result;

    uint32_t line = 1;
    uint16_t column = 1;
    size_t index = 0;

    while (index < source.bytes.size()) {
        const char current = source.bytes[index];

        if (current == ' ' || current == '\t') {
            ++index;
            ++column;
            continue;
        }

        if (current == '\r' || current == '\n') {
            const uint32_t token_offset = static_cast<uint32_t>(index);
            const uint16_t token_column = column;
            uint16_t token_length = 1;
            if (current == '\r' && index + 1 < source.bytes.size() && source.bytes[index + 1] == '\n') {
                ++index;
                ++token_length;
            }

            push_token(result.stream, TokenKind::Newline, token_offset, token_length, line, token_column);
            ++index;
            ++line;
            column = 1;
            continue;
        }

        const uint32_t token_offset = static_cast<uint32_t>(index);
        const uint16_t token_column = column;

        if (is_note_letter(current) && index + 2 < source.bytes.size()) {
            const char accidental = source.bytes[index + 1];
            const char octave = source.bytes[index + 2];
            const char boundary = index + 3 < source.bytes.size() ? source.bytes[index + 3] : '\0';
            if ((accidental == '#' || accidental == '-') &&
                std::isdigit(static_cast<unsigned char>(octave)) != 0 &&
                is_token_boundary(boundary)) {
                push_token(result.stream, TokenKind::Note, token_offset, 3, line, token_column);
                index += 3;
                column += 3;
                continue;
            }
        }

        if (current == '#') {
            size_t comment_end = index + 1;
            while (comment_end < source.bytes.size()) {
                const char comment_char = source.bytes[comment_end];
                if (comment_char == '\r' || comment_char == '\n') {
                    break;
                }
                ++comment_end;
            }

            const auto token_length = static_cast<uint16_t>(comment_end - index);
            push_token(result.stream, TokenKind::Comment, token_offset, token_length, line, token_column);
            column += token_length;
            index = comment_end;
            continue;
        }

        if (current == '-' && index + 2 < source.bytes.size() && source.bytes[index + 1] == '-' &&
            source.bytes[index + 2] == '-') {
            const char boundary = index + 3 < source.bytes.size() ? source.bytes[index + 3] : '\0';
            if (is_token_boundary(boundary)) {
                push_token(result.stream, TokenKind::TripleDash, token_offset, 3, line, token_column);
                index += 3;
                column += 3;
                continue;
            }
        }

        if (current == '-' && index + 1 < source.bytes.size() && source.bytes[index + 1] == '-') {
            const char boundary = index + 2 < source.bytes.size() ? source.bytes[index + 2] : '\0';
            if (is_token_boundary(boundary)) {
                push_token(result.stream, TokenKind::DoubleDash, token_offset, 2, line, token_column);
                index += 2;
                column += 2;
                continue;
            }
        }

        if (is_hex_digit(current) && index + 1 < source.bytes.size() && is_hex_digit(source.bytes[index + 1])) {
            const char boundary = index + 2 < source.bytes.size() ? source.bytes[index + 2] : '\0';
            if (is_token_boundary(boundary)) {
                push_token(result.stream, TokenKind::HexByte, token_offset, 2, line, token_column);
                index += 2;
                column += 2;
                continue;
            }
        }

        if (is_word_letter(current)) {
            size_t word_end = index + 1;
            while (word_end < source.bytes.size() && is_word_letter(source.bytes[word_end])) {
                ++word_end;
            }

            const char boundary = word_end < source.bytes.size() ? source.bytes[word_end] : '\0';
            const auto token_length = static_cast<uint16_t>(word_end - index);
            if (is_token_boundary(boundary)) {
                push_token(result.stream, TokenKind::Word, token_offset, token_length, line, token_column);
            } else {
                size_t invalid_end = word_end;
                while (invalid_end < source.bytes.size() && !is_token_boundary(source.bytes[invalid_end])) {
                    ++invalid_end;
                }
                const auto invalid_length = static_cast<uint16_t>(invalid_end - index);
                push_invalid_token(
                    result,
                    token_offset,
                    invalid_length,
                    line,
                    token_column,
                    DiagnosticKind::InvalidToken);
                column += invalid_length;
                index = invalid_end;
                continue;
            }

            index = word_end;
            column += token_length;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(current)) != 0) {
            size_t invalid_end = index + 1;
            while (invalid_end < source.bytes.size() && !is_token_boundary(source.bytes[invalid_end])) {
                ++invalid_end;
            }

            const auto invalid_length = static_cast<uint16_t>(invalid_end - index);
            push_invalid_token(
                result,
                token_offset,
                invalid_length,
                line,
                token_column,
                DiagnosticKind::InvalidToken);
            column += invalid_length;
            index = invalid_end;
            continue;
        }

        push_invalid_token(
            result,
            token_offset,
            1,
            line,
            token_column,
            DiagnosticKind::InvalidCharacter);
        ++index;
        ++column;
    }

    push_token(
        result.stream,
        TokenKind::EndOfFile,
        static_cast<uint32_t>(source.bytes.size()),
        0,
        line,
        column);

    return result;
}

std::string_view token_text(const SourceText& source, const TokenStream& stream, size_t index) {
    return std::string_view(source.bytes.data() + stream.offset[index], stream.length[index]);
}

}  // namespace gaga
