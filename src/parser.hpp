#pragma once

#include <vector>

#include "pattern.hpp"
#include "source_text.hpp"
#include "tokenizer.hpp"

namespace gaga {

struct ParseResult {
    PatternData pattern;
    std::vector<Diagnostic> diagnostics;
};

ParseResult parse_pattern(const SourceText& source, const TokenStream& stream);

}  // namespace gaga
