#include "tokenizer.hpp"

#include <cctype>

namespace gaga {

namespace {

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
    return value >= 'A' && value <= 'G';
}

bool is_token_boundary(char value) {
    return value == '\0' || value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '#';
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

        if (current == '#') {
            const bool sharp_for_note =
                !result.stream.kind.empty() &&
                result.stream.kind.back() == TokenKind::NoteLetter &&
                static_cast<size_t>(result.stream.offset.back() + result.stream.length.back()) == index;

            if (sharp_for_note) {
                push_token(result.stream, TokenKind::Sharp, token_offset, 1, line, token_column);
                ++index;
                ++column;
                continue;
            }

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

        if (current == 'O' && index + 2 < source.bytes.size() && source.bytes[index + 1] == 'F' &&
            source.bytes[index + 2] == 'F') {
            const char boundary = index + 3 < source.bytes.size() ? source.bytes[index + 3] : '\0';
            if (is_token_boundary(boundary)) {
                push_token(result.stream, TokenKind::Off, token_offset, 3, line, token_column);
                index += 3;
                column += 3;
                continue;
            }
        }

        if (is_note_letter(current)) {
            push_token(result.stream, TokenKind::NoteLetter, token_offset, 1, line, token_column);
            ++index;
            ++column;
            continue;
        }

        if (current == '-') {
            push_token(result.stream, TokenKind::Dash, token_offset, 1, line, token_column);
            ++index;
            ++column;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(current)) != 0) {
            push_token(result.stream, TokenKind::Digit, token_offset, 1, line, token_column);
            ++index;
            ++column;
            continue;
        }

        push_token(result.stream, TokenKind::Invalid, token_offset, 1, line, token_column);
        result.diagnostics.push_back(
            Diagnostic{DiagnosticKind::InvalidCharacter, line, token_column, token_offset, 1});
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
