#include "source_text.hpp"

#include <fstream>

namespace gaga {

tl::expected<SourceText, FileError> load_source_text(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return tl::unexpected(FileError{FileErrorKind::OpenFailed, path, "could not open file"});
    }

    input.seekg(0, std::ios::end);
    const std::streamsize size = input.tellg();
    if (size < 0) {
        return tl::unexpected(FileError{FileErrorKind::ReadFailed, path, "could not determine file size"});
    }

    input.seekg(0, std::ios::beg);

    SourceText source;
    source.path = path;
    source.bytes.resize(static_cast<size_t>(size));
    if (size > 0 && !input.read(source.bytes.data(), size)) {
        return tl::unexpected(FileError{FileErrorKind::ReadFailed, path, "could not read file bytes"});
    }

    return source;
}

}  // namespace gaga
