#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "audio.hpp"
#include "diagnostics.hpp"
#include "file_watch.hpp"
#include "parser.hpp"
#include "pattern.hpp"
#include "source_text.hpp"
#include "tokenizer.hpp"

namespace gaga {

namespace {

struct CliOptions {
    std::string path;
    int bpm = 120;
    int lpb = 4;
    bool loop = false;
    bool trace = false;
};

tl::expected<CliOptions, std::string> parse_cli(int argc, char** argv) {
    CliOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--loop") {
            options.loop = true;
            continue;
        }

        if (argument == "--trace") {
            options.trace = true;
            continue;
        }

        if (argument == "--bpm" || argument == "--lpb") {
            if (index + 1 >= argc) {
                return tl::unexpected(std::string("missing value for ") + argument);
            }

            const int parsed = std::atoi(argv[++index]);
            if (parsed <= 0) {
                return tl::unexpected(std::string("invalid value for ") + argument);
            }

            if (argument == "--bpm") {
                options.bpm = parsed;
            } else {
                options.lpb = parsed;
            }
            continue;
        }

        if (!argument.empty() && argument[0] == '-') {
            return tl::unexpected(std::string("unknown flag: ") + argument);
        }

        if (!options.path.empty()) {
            return tl::unexpected(std::string("expected a single input file"));
        }

        options.path = argument;
    }

    if (options.path.empty()) {
        return tl::unexpected(std::string("usage: gaga <file.gaga> [--loop] [--trace] [--bpm N] [--lpb N]"));
    }

    return options;
}

struct SnapshotLoadResult {
    SourceText source;
    std::shared_ptr<PatternSnapshot> snapshot;
};

tl::expected<SnapshotLoadResult, std::vector<Diagnostic>> load_snapshot_with_source(const std::string& path) {
    auto source_result = load_source_text(path);
    if (!source_result) {
        std::cerr << "error: " << path << ": " << source_result.error().detail << "\n";
        return tl::unexpected(std::vector<Diagnostic>{});
    }

    auto metadata_result = read_file_metadata(path);
    const FileMetadata metadata = metadata_result ? metadata_result.value() : FileMetadata{};
    SourceText source = std::move(source_result.value());

    auto tokenization = tokenize(source);
    auto parse = parse_pattern(source, tokenization.stream);

    std::vector<Diagnostic> diagnostics = std::move(tokenization.diagnostics);
    diagnostics.insert(diagnostics.end(), parse.diagnostics.begin(), parse.diagnostics.end());
    if (!diagnostics.empty()) {
        std::sort(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& left, const Diagnostic& right) { return left.offset < right.offset; });
        return tl::unexpected(std::move(diagnostics));
    }

    auto snapshot = std::make_shared<PatternSnapshot>(
        build_snapshot(std::move(parse.pattern), metadata.timestamp, metadata.size));
    return SnapshotLoadResult{std::move(source), std::move(snapshot)};
}

std::string runtime_error_message(RuntimeErrorKind kind) {
    switch (kind) {
    case RuntimeErrorKind::FileOpenFailed:
        return "file open failed";
    case RuntimeErrorKind::FileReadFailed:
        return "file read failed";
    case RuntimeErrorKind::FileWatchFailed:
        return "file watch failed";
    case RuntimeErrorKind::AudioInitFailed:
        return "audio device initialization failed";
    case RuntimeErrorKind::AudioStartFailed:
        return "audio device start failed";
    case RuntimeErrorKind::AudioDeviceLost:
        return "audio device lost";
    case RuntimeErrorKind::ReloadFailed:
        return "reload failed";
    case RuntimeErrorKind::InternalStateError:
        return "internal playback state error";
    }

    return "unknown runtime error";
}

void print_trace_rows(std::ostream& out, const PatternSnapshot& snapshot) {
    for (size_t row = 0; row < snapshot.pattern.row_count(); ++row) {
        switch (snapshot.pattern.op[row]) {
        case RowOp::Empty:
            break;
        case RowOp::NoteOff:
            out << "row " << std::setw(3) << std::setfill('0') << row << ": note off\n";
            break;
        case RowOp::NoteOn:
            out << "row " << std::setw(3) << std::setfill('0') << row << ": note on "
                << row_to_string(snapshot.pattern.op[row], snapshot.pattern.note_index[row]) << " ("
                << std::fixed << std::setprecision(2)
                << snapshot.frequency_hz[snapshot.pattern.note_index[row]] << " Hz)\n";
            break;
        }
    }
    out << std::setfill(' ') << std::defaultfloat;
}

int run(const CliOptions& options) {
    auto initial = load_snapshot_with_source(options.path);
    if (!initial) {
        auto source = load_source_text(options.path);
        if (source) {
            for (const auto& diagnostic : initial.error()) {
                print_diagnostic(std::cerr, source.value(), diagnostic);
            }
        }
        return 1;
    }

    print_normalized_rows(std::cout, initial.value().snapshot->pattern);
    if (options.trace) {
        print_trace_rows(std::cout, *initial.value().snapshot);
    }

    AudioEngine engine;
    if (const auto init_audio =
            initialize_audio_engine(engine, initial.value().snapshot, options.bpm, options.lpb, options.loop);
        !init_audio) {
        std::cerr << "fatal: " << runtime_error_message(init_audio.error()) << "\n";
        return 2;
    }

    if (const auto start_audio = start_audio_engine(engine); !start_audio) {
        std::cerr << "fatal: " << runtime_error_message(start_audio.error()) << "\n";
        stop_audio_engine(engine);
        return 2;
    }

    FileMetadata observed_metadata{
        initial.value().snapshot->file_timestamp,
        initial.value().snapshot->file_size,
    };
    FileMetadata warned_metadata{};
    bool file_watch_warning_reported = false;

    while (true) {
        RuntimeEvent event;
        while (pop_runtime_event(engine.runtime_events, event)) {
            if (event.severity == RuntimeSeverity::Fatal) {
                std::cerr << "fatal: " << runtime_error_message(event.kind) << "\n";
                stop_audio_engine(engine);
                return 3;
            }

            std::cerr << "warning: " << runtime_error_message(event.kind) << "\n";
        }

        if (engine.shutdown_requested.load(std::memory_order_acquire)) {
            std::cerr << "fatal: " << runtime_error_message(RuntimeErrorKind::InternalStateError) << "\n";
            stop_audio_engine(engine);
            return 3;
        }

        if (engine.playback_finished.load(std::memory_order_acquire)) {
            stop_audio_engine(engine);
            return 0;
        }

        if (options.loop) {
            auto metadata_result = read_file_metadata(options.path);
            if (!metadata_result) {
                if (!file_watch_warning_reported) {
                    std::cerr << "warning: reload failed, continuing with previous pattern\n";
                    file_watch_warning_reported = true;
                }
            } else if (metadata_result.value().timestamp != observed_metadata.timestamp ||
                       metadata_result.value().size != observed_metadata.size) {
                file_watch_warning_reported = false;
                observed_metadata = metadata_result.value();
                auto reload = load_snapshot_with_source(options.path);
                if (!reload) {
                    if (warned_metadata.timestamp != observed_metadata.timestamp ||
                        warned_metadata.size != observed_metadata.size) {
                        auto source = load_source_text(options.path);
                        if (source) {
                            for (const auto& diagnostic : reload.error()) {
                                print_diagnostic(std::cerr, source.value(), diagnostic);
                            }
                        }
                        std::cerr << "warning: reload failed, continuing with previous pattern\n";
                        warned_metadata = observed_metadata;
                    }
                } else {
                    warned_metadata = FileMetadata{};
                    store_pending_snapshot(engine, reload.value().snapshot);
                }
            } else {
                file_watch_warning_reported = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

}  // namespace

}  // namespace gaga

int main(int argc, char** argv) {
    const auto options = gaga::parse_cli(argc, argv);
    if (!options) {
        std::cerr << options.error() << "\n";
        return 1;
    }

    return gaga::run(options.value());
}
