#include <filesystem>
#include <fstream>
#include <iostream>

#include "audio_debug.hpp"
#include "transport.hpp"

namespace {

gaga::PatternSnapshot make_snapshot(std::initializer_list<gaga::RowOp> ops) {
    gaga::PatternData pattern;
    pattern.op.assign(ops.begin(), ops.end());
    pattern.note_index.resize(pattern.op.size(), 0);
    pattern.source_line.resize(pattern.op.size(), 0);

    std::vector<std::string> source_lines;
    source_lines.reserve(pattern.op.size());

    uint32_t line = 1;
    for (size_t index = 0; index < pattern.op.size(); ++index) {
        pattern.source_line[index] = line++;
        switch (pattern.op[index]) {
        case gaga::RowOp::Empty:
            source_lines.emplace_back("---");
            break;
        case gaga::RowOp::NoteOn:
            pattern.note_index[index] = 48;
            source_lines.emplace_back("C-4");
            break;
        case gaga::RowOp::NoteOff:
            source_lines.emplace_back("OFF");
            break;
        }
    }

    return gaga::build_snapshot(std::move(pattern), std::move(source_lines), 0, 0, 1);
}

bool test_audio_summary_counts() {
    const auto snapshot = make_snapshot({gaga::RowOp::NoteOn, gaga::RowOp::Empty, gaga::RowOp::NoteOff});

    gaga::AudioDebugConfig config;
    config.bpm = 120;
    config.lpb = 4;
    config.sample_rate = 48000;
    config.channels = 2;

    const auto rendered = gaga::render_pattern_audio_debug(snapshot, config, false);
    const auto expected_frames =
        static_cast<uint64_t>(snapshot.pattern.row_count()) * gaga::compute_frames_per_row(48000, 120, 4);

    if (rendered.summary.rendered_frames != expected_frames) {
        std::cerr << "unexpected rendered frame count\n";
        return false;
    }

    if (rendered.summary.note_on_events != 1 || rendered.summary.note_off_events != 1 ||
        rendered.summary.empty_rows != 1) {
        std::cerr << "unexpected row event counts\n";
        return false;
    }

    if (rendered.summary.peak_abs <= 0.0f || rendered.summary.non_finite_frames != 0) {
        std::cerr << "unexpected signal metrics\n";
        return false;
    }

    return true;
}

bool test_wav_export() {
    const auto snapshot = make_snapshot({gaga::RowOp::NoteOn, gaga::RowOp::NoteOff});

    gaga::AudioDebugConfig config;
    const auto rendered = gaga::render_pattern_audio_debug(snapshot, config, true);

    const auto path = std::filesystem::path("gaga_audio_debug_test.wav");
    std::error_code remove_error;
    std::filesystem::remove(path, remove_error);

    const auto write_result = gaga::write_rendered_wav(path.string(), rendered, config);
    if (!write_result) {
        std::cerr << "wav export failed\n";
        return false;
    }

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "wav file was not created\n";
        return false;
    }

    const auto size = in.tellg();
    in.close();
    std::filesystem::remove(path, remove_error);

    if (size <= 44) {
        std::cerr << "wav file too small\n";
        return false;
    }

    return true;
}

}  // namespace

int main() {
    if (!test_audio_summary_counts()) {
        return 1;
    }

    if (!test_wav_export()) {
        return 1;
    }

    return 0;
}
