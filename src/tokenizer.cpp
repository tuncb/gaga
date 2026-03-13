#include "tokenizer.hpp"

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

bool is_token_boundary(char value) {
    return value == '\0' || value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '#';
}

bool is_atom_boundary(char value) {
    return value == '\0' || value == ' ' || value == '\t' || value == '\r' || value == '\n';
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

        size_t atom_end = index + 1;
        while (atom_end < source.bytes.size() && !is_atom_boundary(source.bytes[atom_end])) {
            ++atom_end;
        }

        const auto token_length = static_cast<uint16_t>(atom_end - index);
        push_token(result.stream, TokenKind::Atom, token_offset, token_length, line, token_column);
        index = atom_end;
        column += token_length;
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
