#include "file_watch.hpp"

#include <chrono>
#include <filesystem>

namespace gaga {

tl::expected<FileMetadata, FileWatchError> read_file_metadata(const std::string& path) {
    namespace fs = std::filesystem;

    std::error_code error;
    const auto timestamp = fs::last_write_time(path, error);
    if (error) {
        return tl::unexpected(FileWatchError{path, error.message()});
    }

    const auto size = fs::file_size(path, error);
    if (error) {
        return tl::unexpected(FileWatchError{path, error.message()});
    }

    const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp.time_since_epoch()).count();
    return FileMetadata{static_cast<uint64_t>(ticks), static_cast<uint64_t>(size)};
}

}  // namespace gaga
