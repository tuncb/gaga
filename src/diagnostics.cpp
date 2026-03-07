#include "diagnostics.hpp"

#include <algorithm>
#include <ostream>
#include <string>

namespace gaga {

std::string diagnostic_message(DiagnosticKind kind) {
    switch (kind) {
    case DiagnosticKind::InvalidCharacter:
        return "invalid character";
    case DiagnosticKind::InvalidToken:
        return "invalid token";
    case DiagnosticKind::UnexpectedToken:
        return "unexpected token";
    case DiagnosticKind::TrailingTokens:
        return "trailing tokens";
    case DiagnosticKind::NoteOutOfRange:
        return "note out of range";
    }

    return "unknown diagnostic";
}

std::string_view line_text_for_offset(const SourceText& source, uint32_t offset) {
    const auto safe_offset = std::min<size_t>(offset, source.bytes.size());

    size_t begin = safe_offset;
    while (begin > 0) {
        const char previous = source.bytes[begin - 1];
        if (previous == '\n' || previous == '\r') {
            break;
        }
        --begin;
    }

    size_t end = safe_offset;
    while (end < source.bytes.size()) {
        const char current = source.bytes[end];
        if (current == '\n' || current == '\r') {
            break;
        }
        ++end;
    }

    return std::string_view(source.bytes.data() + begin, end - begin);
}

void print_diagnostic(std::ostream& out, const SourceText& source, const Diagnostic& diagnostic) {
    out << "error: " << source.path << ":" << diagnostic.line << ":" << diagnostic.column << ": "
        << diagnostic_message(diagnostic.kind) << "\n";

    const auto line = line_text_for_offset(source, diagnostic.offset);
    if (line.empty()) {
        return;
    }

    out << line << "\n";
    out << std::string(diagnostic.column > 0 ? diagnostic.column - 1 : 0, ' ');
    out << std::string(std::max<uint16_t>(1, diagnostic.length), '^') << "\n";
}

}  // namespace gaga
