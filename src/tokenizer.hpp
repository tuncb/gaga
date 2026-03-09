#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "diagnostics.hpp"
#include "source_text.hpp"

namespace gaga {

enum class TokenKind : uint8_t {
    Note,
    TripleDash,
    Word,
    HexByte,
    Comment,
    Newline,
    EndOfFile,
    Invalid
};

struct TokenStream {
    std::vector<TokenKind> kind;
    std::vector<uint32_t> offset;
    std::vector<uint16_t> length;
    std::vector<uint32_t> line;
    std::vector<uint16_t> column;

    [[nodiscard]] size_t size() const { return kind.size(); }
};

struct TokenizationResult {
    TokenStream stream;
    std::vector<Diagnostic> diagnostics;
};

TokenizationResult tokenize(const SourceText& source);
std::string_view token_text(const SourceText& source, const TokenStream& stream, size_t index);

}  // namespace gaga
