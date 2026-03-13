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

bool test_volume_and_instrument_columns_update_voice_state() {
    gaga::PatternData pattern;
    pattern.op = {gaga::RowOp::NoteOn, gaga::RowOp::NoteOn, gaga::RowOp::NoteOn, gaga::RowOp::NoteOff};
    pattern.note_index = {48, 50, 52, 0};
    pattern.source_line = {1, 2, 3, 4};
    pattern.row_columns = {
        static_cast<uint8_t>(gaga::kRowColumnVolume | gaga::kRowColumnInstrument),
        0,
        gaga::kRowColumnInstrument,
        0,
    };
    pattern.volume = {0x40, 0, 0, 0};
    pattern.instrument = {0x02, 0, 0x03, 0};
    pattern.fx_start = {0, 0, 0, 0};
    pattern.fx_count = {0, 0, 0, 0};

    std::vector<std::string> source_lines{
        "C-4 40 02",
        "D-4",
        "E-4 -- 03",
        "OFF",
    };

    const auto snapshot = gaga::build_snapshot(std::move(pattern), std::move(source_lines), 0, 0, 1);

    gaga::TransportState transport;
    gaga::SynthVoice voice;
    float master_gain = 1.0f;
    gaga::initialize_transport(transport, 48000, 120, 4, false, true);

    gaga::apply_row_event(
        snapshot.pattern,
        0,
        transport,
        voice,
        master_gain,
        gaga::SynthType::Square,
        snapshot.frequency_hz,
        48000);
    if (!voice.active || voice.type != gaga::SynthType::Saw) {
        std::cerr << "expected instrument column to select saw on row 0\n";
        return false;
    }

    if (!approximately_equal(voice.note_gain, 64.0f / 127.0f, 1.0e-4f)) {
        std::cerr << "unexpected absolute note gain from volume column\n";
        return false;
    }

    voice.phase = 1.25f;
    gaga::apply_row_event(
        snapshot.pattern,
        1,
        transport,
        voice,
        master_gain,
        gaga::SynthType::Square,
        snapshot.frequency_hz,
        48000);
    if (voice.type != gaga::SynthType::Saw || !approximately_equal(voice.phase, 1.25f)) {
        std::cerr << "expected note row without instrument column to stay legato\n";
        return false;
    }

    voice.phase = 2.0f;
    gaga::apply_row_event(
        snapshot.pattern,
        2,
        transport,
        voice,
        master_gain,
        gaga::SynthType::Square,
        snapshot.frequency_hz,
        48000);
    if (voice.type != gaga::SynthType::Triangle || !approximately_equal(voice.phase, 0.0f)) {
        std::cerr << "expected instrument column to retrigger with triangle on row 2\n";
        return false;
    }

    return true;
}

bool test_apply_row_fx_updates_voice_state() {
    gaga::PatternData pattern;
    pattern.op = {gaga::RowOp::NoteOn, gaga::RowOp::Empty, gaga::RowOp::Empty, gaga::RowOp::NoteOff};
    pattern.note_index = {48, 0, 0, 0};
    pattern.source_line = {1, 2, 3, 4};
    pattern.fx_start = {0, 1, 3, 6};
    pattern.fx_count = {1, 2, 3, 0};
    pattern.fx_command = {
        gaga::FxCommand::Volume,
        gaga::FxCommand::Pitch,
        gaga::FxCommand::Fine,
        gaga::FxCommand::Transpose,
        gaga::FxCommand::Tempo,
        gaga::FxCommand::MasterVolume,
    };
    pattern.fx_value = {0x20, 0x01, 0x40, 0xFF, 0x90, 0x80};

    std::vector<std::string> source_lines{
        "C-4 VOL 20",
        "--- PIT 01 FIN 40",
        "--- TSP FF TPO 90 VMV 80",
        "OFF",
    };

    const auto snapshot = gaga::build_snapshot(std::move(pattern), std::move(source_lines), 0, 0, 1);

    gaga::TransportState transport;
    gaga::SynthVoice voice;
    float master_gain = 1.0f;
    gaga::initialize_transport(transport, 48000, 120, 4, false, true);
    gaga::apply_row_event(
        snapshot.pattern,
        0,
        transport,
        voice,
        master_gain,
        gaga::SynthType::Sine,
        snapshot.frequency_hz,
        48000);
    if (!voice.active) {
        std::cerr << "expected row 0 to activate the voice\n";
        return false;
    }

    if (!approximately_equal(voice.gain_scale, 1.5f)) {
        std::cerr << "unexpected volume gain scale\n";
        return false;
    }

    gaga::apply_row_event(
        snapshot.pattern,
        1,
        transport,
        voice,
        master_gain,
        gaga::SynthType::Sine,
        snapshot.frequency_hz,
        48000);
    const float expected_frequency_after_pitch =
        gaga::note_index_to_frequency(48) * std::pow(2.0f, 1.5f / 12.0f);
    const float expected_phase_step_after_pitch = kTwoPi * expected_frequency_after_pitch / 48000.0f;

    if (!approximately_equal(voice.phase_step, expected_phase_step_after_pitch)) {
        std::cerr << "unexpected pitch phase step after PIT/FIN\n";
        return false;
    }

    gaga::apply_row_event(
        snapshot.pattern,
        2,
        transport,
        voice,
        master_gain,
        gaga::SynthType::Sine,
        snapshot.frequency_hz,
        48000);
    const float expected_frequency_after_transpose =
        gaga::note_index_to_frequency(48) * std::pow(2.0f, 0.5f / 12.0f);
    const float expected_phase_step_after_transpose = kTwoPi * expected_frequency_after_transpose / 48000.0f;

    if (!approximately_equal(voice.phase_step, expected_phase_step_after_transpose)) {
        std::cerr << "unexpected pitch phase step after TSP\n";
        return false;
    }

    if (transport.bpm != 0x90 || transport.frames_per_row != gaga::compute_frames_per_row(48000, 0x90, 4)) {
        std::cerr << "unexpected transport tempo after TPO\n";
        return false;
    }

    if (!approximately_equal(master_gain, 128.0f / 255.0f, 1.0e-3f)) {
        std::cerr << "unexpected master gain after VMV\n";
        return false;
    }

    gaga::apply_row_event(
        snapshot.pattern,
        3,
        transport,
        voice,
        master_gain,
        gaga::SynthType::Sine,
        snapshot.frequency_hz,
        48000);
    if (voice.active) {
        std::cerr << "expected OFF row to deactivate the voice\n";
        return false;
    }

    return true;
}

bool test_audio_summary_respects_tpo_and_vmv() {
    gaga::PatternData pattern;
    pattern.op = {gaga::RowOp::NoteOn, gaga::RowOp::Empty, gaga::RowOp::NoteOff};
    pattern.note_index = {48, 0, 0};
    pattern.source_line = {1, 2, 3};
    pattern.fx_start = {0, 1, 3};
    pattern.fx_count = {1, 2, 0};
    pattern.fx_command = {
        gaga::FxCommand::MasterVolume,
        gaga::FxCommand::Tempo,
        gaga::FxCommand::MasterVolume,
    };
    pattern.fx_value = {0x40, 0xF0, 0x80};

    std::vector<std::string> source_lines{
        "C-4 VMV 40",
        "--- TPO F0 VMV 80",
        "OFF",
    };

    const auto snapshot = gaga::build_snapshot(std::move(pattern), std::move(source_lines), 0, 0, 1);
    gaga::AudioDebugConfig config;
    config.bpm = 120;
    config.lpb = 4;
    config.sample_rate = 48000;
    config.channels = 2;
    config.synth_type = gaga::SynthType::Square;

    const auto rendered = gaga::render_pattern_audio_debug(snapshot, config, false);
    const uint64_t expected_frames =
        static_cast<uint64_t>(gaga::compute_frames_per_row(48000, 120, 4)) +
        static_cast<uint64_t>(gaga::compute_frames_per_row(48000, 0xF0, 4)) +
        static_cast<uint64_t>(gaga::compute_frames_per_row(48000, 0xF0, 4));

    if (rendered.summary.rendered_frames != expected_frames) {
        std::cerr << "unexpected rendered frame count with TPO\n";
        return false;
    }

    if (rendered.summary.peak_abs >= 0.15f) {
        std::cerr << "expected VMV to reduce peak amplitude\n";
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

    if (!test_volume_and_instrument_columns_update_voice_state()) {
        return 1;
    }

    if (!test_apply_row_fx_updates_voice_state()) {
        return 1;
    }

    if (!test_audio_summary_respects_tpo_and_vmv()) {
        return 1;
    }

    if (!test_wav_export()) {
        return 1;
    }

    return 0;
}
