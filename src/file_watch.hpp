#pragma once

#include <cstdint>
#include <string>

#include <tl/expected.hpp>

namespace gaga {

struct FileMetadata {
    uint64_t timestamp = 0;
    uint64_t size = 0;
};

struct FileWatchError {
    std::string path;
    std::string detail;
};

tl::expected<FileMetadata, FileWatchError> read_file_metadata(const std::string& path);

}  // namespace gaga
