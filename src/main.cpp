#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <tl/expected.hpp>

#include "audio.hpp"
#include "audio_debug.hpp"
#include "diagnostics.hpp"
#include "file_watch.hpp"
#include "parser.hpp"
#include "pattern.hpp"
#include "source_text.hpp"
#include "terminal_display.hpp"
#include "tokenizer.hpp"
#include "version.hpp"

namespace gaga {

namespace {

struct CliOptions {
    std::string path;
    std::string render_wav_path;
    int bpm = 120;
    int lpb = 4;
    SynthType synth_type = SynthType::Sine;
    bool loop = false;
    bool trace = false;
    bool analyze_audio = false;
    bool show_help = false;
    bool show_version = false;
};

std::string usage_line() {
    return "usage: gaga <file.gaga> [--loop] [--trace] [--analyze-audio] [--render-wav PATH] [--bpm N] [--lpb N] [--synth NAME]";
}

void print_help(std::ostream& out) {
    out << "gaga " << version_string() << "\n";
    out << usage_line() << "\n\n";
    out << "options:\n";
    out << "  --help       show this help message\n";
    out << "  --version    show version information\n";
    out << "  --loop       reload the file while playing\n";
    out << "  --trace      print normalized playback rows\n";
    out << "  --analyze-audio\n";
    out << "               render one offline pass, print signal diagnostics, and exit\n";
    out << "  --render-wav PATH\n";
    out << "               render one offline pass to a wav file and exit\n";
    out << "  --bpm N      set beats per minute (default: 120)\n";
    out << "  --lpb N      set lines per beat (default: 4)\n";
    out << "  --synth NAME set synth waveform: sine, square, saw, triangle, noise\n";
    out << "\n";
    out << "runtime:\n";
    out << "  Esc          stop playback and exit\n";
}

tl::expected<CliOptions, std::string> parse_cli(int argc, char** argv) {
    CliOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            options.show_help = true;
            continue;
        }

        if (argument == "--version") {
            options.show_version = true;
            continue;
        }

        if (argument == "--loop") {
            options.loop = true;
            continue;
        }

        if (argument == "--trace") {
            options.trace = true;
            continue;
        }

        if (argument == "--analyze-audio") {
            options.analyze_audio = true;
            continue;
        }

        if (argument == "--render-wav") {
            if (index + 1 >= argc) {
                return tl::unexpected(std::string("missing value for ") + argument);
            }

            options.render_wav_path = argv[++index];
            continue;
        }

        if (argument == "--synth") {
            if (index + 1 >= argc) {
                return tl::unexpected(std::string("missing value for ") + argument);
            }

            const auto synth_type = parse_synth_type(argv[++index]);
            if (!synth_type) {
                return tl::unexpected(synth_type.error());
            }

            options.synth_type = synth_type.value();
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

    if (options.show_help || options.show_version) {
        return options;
    }

    if (options.path.empty()) {
        return tl::unexpected(usage_line());
    }

    if (!options.render_wav_path.empty() && options.loop) {
        return tl::unexpected(std::string("--render-wav cannot be used with --loop"));
    }

    return options;
}

struct SnapshotLoadResult {
    SourceText source;
    std::shared_ptr<PatternSnapshot> snapshot;
};

std::vector<std::string> split_source_lines(const SourceText& source) {
    std::vector<std::string> lines;
    std::string current_line;

    size_t index = 0;
    while (index < source.bytes.size()) {
        const char current = source.bytes[index];
        if (current == '\r' || current == '\n') {
            lines.push_back(current_line);
            current_line.clear();

            if (current == '\r' && index + 1 < source.bytes.size() && source.bytes[index + 1] == '\n') {
                ++index;
            }
        } else {
            current_line.push_back(current);
        }

        ++index;
    }

    if (!current_line.empty() ||
        (!source.bytes.empty() && source.bytes.back() != '\r' && source.bytes.back() != '\n')) {
        lines.push_back(std::move(current_line));
    }

    return lines;
}

tl::expected<SnapshotLoadResult, std::vector<Diagnostic>> load_snapshot_with_source(
    const std::string& path,
    uint32_t display_generation) {
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
        build_snapshot(
            std::move(parse.pattern),
            split_source_lines(source),
            metadata.timestamp,
            metadata.size,
            display_generation));
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
    uint32_t next_display_generation = 1;
    auto initial = load_snapshot_with_source(options.path, next_display_generation++);
    if (!initial) {
        auto source = load_source_text(options.path);
        if (source) {
            for (const auto& diagnostic : initial.error()) {
                print_diagnostic(std::cerr, source.value(), diagnostic);
            }
        }
        return 1;
    }

    AudioDebugConfig debug_config;
    debug_config.bpm = options.bpm;
    debug_config.lpb = options.lpb;
    debug_config.synth_type = options.synth_type;

    if (options.analyze_audio || !options.render_wav_path.empty()) {
        const bool capture_samples = !options.render_wav_path.empty();
        const auto rendered = render_pattern_audio_debug(*initial.value().snapshot, debug_config, capture_samples);

        if (options.analyze_audio) {
            print_audio_debug_summary(std::cout, *initial.value().snapshot, debug_config, rendered.summary);
        }

        if (!options.render_wav_path.empty()) {
            const auto write_wav = write_rendered_wav(options.render_wav_path, rendered, debug_config);
            if (!write_wav) {
                std::cerr << "error: " << write_wav.error() << "\n";
                return 1;
            }

            std::cout << "wrote wav debug render: " << options.render_wav_path << "\n";
        }

        return 0;
    }

    TerminalDisplay display;
    const bool live_display_enabled = !options.trace && display.initialize(std::cout);
    if (!live_display_enabled) {
        print_normalized_rows(std::cout, initial.value().snapshot->pattern);
    }
    if (!live_display_enabled && options.trace) {
        print_trace_rows(std::cout, *initial.value().snapshot);
    }

    AudioEngine engine;
    if (const auto init_audio =
            initialize_audio_engine(
                engine,
                initial.value().snapshot,
                options.bpm,
                options.lpb,
                options.loop,
                options.synth_type);
        !init_audio) {
        display.shutdown();
        std::cerr << "fatal: " << runtime_error_message(init_audio.error()) << "\n";
        return 2;
    }

    if (const auto start_audio = start_audio_engine(engine); !start_audio) {
        display.shutdown();
        std::cerr << "fatal: " << runtime_error_message(start_audio.error()) << "\n";
        stop_audio_engine(engine);
        return 2;
    }

    std::unordered_map<uint32_t, std::shared_ptr<const PatternSnapshot>> snapshots_by_generation;
    snapshots_by_generation.emplace(
        initial.value().snapshot->display_generation,
        initial.value().snapshot);
    std::shared_ptr<const PatternSnapshot> displayed_snapshot = initial.value().snapshot;

    if (live_display_enabled) {
        const uint64_t display_state = engine.display_state.load(std::memory_order_acquire);
        display.render(*displayed_snapshot, decode_display_row(display_state), true);
    }

    FileMetadata observed_metadata{
        initial.value().snapshot->file_timestamp,
        initial.value().snapshot->file_size,
    };
    FileMetadata warned_metadata{};
    bool file_watch_warning_reported = false;

    while (true) {
        RuntimeEvent event;
        bool force_redraw = false;
        while (pop_runtime_event(engine.runtime_events, event)) {
            if (event.severity == RuntimeSeverity::Fatal) {
                display.shutdown();
                std::cerr << "fatal: " << runtime_error_message(event.kind) << "\n";
                stop_audio_engine(engine);
                return 3;
            }

            std::cerr << "warning: " << runtime_error_message(event.kind) << "\n";
            force_redraw = true;
        }

        if (engine.shutdown_requested.load(std::memory_order_acquire)) {
            display.shutdown();
            std::cerr << "fatal: " << runtime_error_message(RuntimeErrorKind::InternalStateError) << "\n";
            stop_audio_engine(engine);
            return 3;
        }

        if (engine.playback_finished.load(std::memory_order_acquire)) {
            if (live_display_enabled) {
                const uint64_t display_state = engine.display_state.load(std::memory_order_acquire);
                const uint32_t active_generation = decode_display_generation(display_state);
                const auto snapshot_it = snapshots_by_generation.find(active_generation);
                if (snapshot_it != snapshots_by_generation.end()) {
                    displayed_snapshot = snapshot_it->second;
                }
                display.render(*displayed_snapshot, decode_display_row(display_state), true);
            }
            stop_audio_engine(engine);
            return 0;
        }

        if (display.poll_escape_pressed()) {
            display.shutdown();
            stop_audio_engine(engine);
            return 0;
        }

        if (options.loop) {
            auto metadata_result = read_file_metadata(options.path);
            if (!metadata_result) {
                if (!file_watch_warning_reported) {
                    std::cerr << "warning: reload failed, continuing with previous pattern\n";
                    file_watch_warning_reported = true;
                    force_redraw = true;
                }
            } else if (metadata_result.value().timestamp != observed_metadata.timestamp ||
                       metadata_result.value().size != observed_metadata.size) {
                file_watch_warning_reported = false;
                observed_metadata = metadata_result.value();
                auto reload = load_snapshot_with_source(options.path, next_display_generation++);
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
                        force_redraw = true;
                    }
                } else {
                    warned_metadata = FileMetadata{};
                    snapshots_by_generation.emplace(
                        reload.value().snapshot->display_generation,
                        reload.value().snapshot);
                    store_pending_snapshot(engine, reload.value().snapshot);
                }
            } else {
                file_watch_warning_reported = false;
            }
        }

        if (live_display_enabled) {
            const uint64_t display_state = engine.display_state.load(std::memory_order_acquire);
            const uint32_t active_generation = decode_display_generation(display_state);
            const auto snapshot_it = snapshots_by_generation.find(active_generation);
            if (snapshot_it != snapshots_by_generation.end()) {
                if (displayed_snapshot != snapshot_it->second) {
                    displayed_snapshot = snapshot_it->second;
                    force_redraw = true;
                }

                display.render(*displayed_snapshot, decode_display_row(display_state), force_redraw);

                for (auto it = snapshots_by_generation.begin(); it != snapshots_by_generation.end();) {
                    if (it->first < active_generation) {
                        it = snapshots_by_generation.erase(it);
                    } else {
                        ++it;
                    }
                }
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

    const auto& cli = options.value();
    if (cli.show_help) {
        gaga::print_help(std::cout);
        return 0;
    }

    if (cli.show_version) {
        std::cout << gaga::version_string() << "\n";
        return 0;
    }

    return gaga::run(cli);
}
