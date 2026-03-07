#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <tl/expected.hpp>

namespace gaga {

struct SourceText {
    std::string path;
    std::vector<char> bytes;
};

enum class FileErrorKind : uint8_t {
    OpenFailed,
    ReadFailed
};

struct FileError {
    FileErrorKind kind;
    std::string path;
    std::string detail;
};

tl::expected<SourceText, FileError> load_source_text(const std::string& path);

}  // namespace gaga
