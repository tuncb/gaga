#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>

#include "audio_debug.hpp"
#include "note.hpp"
#include "pattern.hpp"
#include "synth.hpp"
#include "transport.hpp"

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

gaga::PatternSnapshot make_snapshot(std::initializer_list<gaga::RowOp> ops) {
    gaga::PatternData pattern;
    pattern.op.assign(ops.begin(), ops.end());
    pattern.note_index.resize(pattern.op.size(), 0);
    pattern.source_line.resize(pattern.op.size(), 0);
    pattern.fx_start.resize(pattern.op.size(), 0);
    pattern.fx_count.resize(pattern.op.size(), 0);

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

bool approximately_equal(float left, float right, float tolerance = 1.0e-4f) {
    return std::abs(left - right) <= tolerance;
}

bool test_audio_summary_counts() {
    const auto snapshot = make_snapshot({gaga::RowOp::NoteOn, gaga::RowOp::Empty, gaga::RowOp::NoteOff});

    gaga::AudioDebugConfig config;
    config.bpm = 120;
    config.lpb = 4;
    config.sample_rate = 48000;
    config.channels = 2;
    config.synth_type = gaga::SynthType::Square;

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

bool test_apply_row_fx_updates_voice_state() {
    gaga::PatternData pattern;
    pattern.op = {gaga::RowOp::NoteOn, gaga::RowOp::Empty, gaga::RowOp::NoteOff};
    pattern.note_index = {48, 0, 0};
    pattern.source_line = {1, 2, 3};
    pattern.fx_start = {0, 1, 3};
    pattern.fx_count = {1, 2, 0};
    pattern.fx_command = {
        gaga::FxCommand::Volume,
        gaga::FxCommand::Pitch,
        gaga::FxCommand::Fine,
    };
    pattern.fx_value = {0x20, 0x01, 0x40};

    std::vector<std::string> source_lines{
        "C-4 VOL 20",
        "--- PIT 01 FIN 40",
        "OFF",
    };

    const auto snapshot = gaga::build_snapshot(std::move(pattern), std::move(source_lines), 0, 0, 1);

    gaga::SynthVoice voice;
    gaga::apply_row_event(snapshot.pattern, 0, voice, gaga::SynthType::Sine, snapshot.frequency_hz, 48000);
    if (!voice.active) {
        std::cerr << "expected row 0 to activate the voice\n";
        return false;
    }

    if (!approximately_equal(voice.gain_scale, 1.5f)) {
        std::cerr << "unexpected volume gain scale\n";
        return false;
    }

    gaga::apply_row_event(snapshot.pattern, 1, voice, gaga::SynthType::Sine, snapshot.frequency_hz, 48000);
    const float expected_frequency =
        gaga::note_index_to_frequency(48) * std::pow(2.0f, 1.5f / 12.0f);
    const float expected_phase_step = kTwoPi * expected_frequency / 48000.0f;

    if (!approximately_equal(voice.phase_step, expected_phase_step)) {
        std::cerr << "unexpected pitch phase step after PIT/FIN\n";
        return false;
    }

    gaga::apply_row_event(snapshot.pattern, 2, voice, gaga::SynthType::Sine, snapshot.frequency_hz, 48000);
    if (voice.active) {
        std::cerr << "expected OFF row to deactivate the voice\n";
        return false;
    }

    return true;
}

bool test_parse_synth_type() {
    const auto saw = gaga::parse_synth_type("saw");
    if (!saw || saw.value() != gaga::SynthType::Saw) {
        std::cerr << "failed to parse valid synth type\n";
        return false;
    }

    const auto invalid = gaga::parse_synth_type("supersaw");
    if (invalid) {
        std::cerr << "unexpected success for invalid synth type\n";
        return false;
    }

    return true;
}

bool test_wav_export() {
    const auto snapshot = make_snapshot({gaga::RowOp::NoteOn, gaga::RowOp::NoteOff});

    gaga::AudioDebugConfig config;
    config.synth_type = gaga::SynthType::Triangle;
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

    if (!test_parse_synth_type()) {
        return 1;
    }

    if (!test_apply_row_fx_updates_voice_state()) {
        return 1;
    }

    if (!test_wav_export()) {
        return 1;
    }

    return 0;
}
