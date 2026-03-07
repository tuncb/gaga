#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

#include "source_text.hpp"

namespace gaga {

enum class DiagnosticKind : uint8_t {
    InvalidCharacter,
    InvalidToken,
    UnexpectedToken,
    TrailingTokens,
    NoteOutOfRange
};

struct Diagnostic {
    DiagnosticKind kind;
    uint32_t line;
    uint16_t column;
    uint32_t offset;
    uint16_t length;
};

std::string diagnostic_message(DiagnosticKind kind);
void print_diagnostic(std::ostream& out, const SourceText& source, const Diagnostic& diagnostic);
std::string_view line_text_for_offset(const SourceText& source, uint32_t offset);

}  // namespace gaga
