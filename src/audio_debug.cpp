#include "audio_debug.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ostream>

#include "transport.hpp"

namespace gaga {

namespace {

void count_row_event(const PatternData& pattern, uint32_t row, AudioDebugSummary& summary) {
    if (row >= pattern.row_count()) {
        return;
    }

    switch (pattern.op[row]) {
    case RowOp::Empty:
        ++summary.empty_rows;
        break;
    case RowOp::NoteOn:
        ++summary.note_on_events;
        break;
    case RowOp::NoteOff:
        ++summary.note_off_events;
        break;
    }
}

void accumulate_frame(
    float sample,
    bool has_previous_sample,
    float previous_sample,
    const AudioDebugConfig& config,
    AudioDebugSummary& summary,
    double& sum,
    double& sum_squares) {
    if (!std::isfinite(sample)) {
        ++summary.non_finite_frames;
        sample = 0.0f;
    }

    const float absolute = std::abs(sample);
    summary.peak_abs = (std::max)(summary.peak_abs, absolute);
    if (absolute >= config.clip_threshold) {
        ++summary.clipped_frames;
    }
    if (absolute <= config.silence_threshold) {
        ++summary.silent_frames;
    }

    if (has_previous_sample) {
        const float delta = std::abs(sample - previous_sample);
        summary.max_delta = (std::max)(summary.max_delta, delta);
        if (delta >= config.discontinuity_threshold) {
            ++summary.discontinuity_count;
        }
    }

    sum += sample;
    sum_squares += static_cast<double>(sample) * static_cast<double>(sample);
    ++summary.rendered_frames;
}

void write_u16(std::ofstream& out, uint16_t value) {
    out.put(static_cast<char>(value & 0xffU));
    out.put(static_cast<char>((value >> 8U) & 0xffU));
}

void write_u32(std::ofstream& out, uint32_t value) {
    out.put(static_cast<char>(value & 0xffU));
    out.put(static_cast<char>((value >> 8U) & 0xffU));
    out.put(static_cast<char>((value >> 16U) & 0xffU));
    out.put(static_cast<char>((value >> 24U) & 0xffU));
}

}  // namespace

RenderedAudio render_pattern_audio_debug(
    const PatternSnapshot& snapshot,
    const AudioDebugConfig& config,
    bool capture_samples) {
    RenderedAudio rendered;

    const uint32_t channels = (std::max)(config.channels, 1U);
    const uint32_t frames_per_row = compute_frames_per_row(config.sample_rate, config.bpm, config.lpb);
    const auto& pattern = snapshot.pattern;
    if (pattern.row_count() == 0) {
        return rendered;
    }

    rendered.summary.rendered_frames = 0;

    TransportState transport;
    SynthVoice voice;
    initialize_transport(transport, frames_per_row, false, true);
    apply_row_event(pattern, 0, voice, snapshot.frequency_hz, config.sample_rate);
    count_row_event(pattern, 0, rendered.summary);

    if (capture_samples) {
        rendered.interleaved_samples.reserve(
            static_cast<size_t>(pattern.row_count()) * frames_per_row * channels);
    }

    double sum = 0.0;
    double sum_squares = 0.0;
    float previous_sample = 0.0f;
    bool has_previous_sample = false;

    while (!transport.finished) {
        for (uint32_t frame = 0; frame < transport.frames_until_row; ++frame) {
            const float sample = next_sample(voice);
            accumulate_frame(
                sample,
                has_previous_sample,
                previous_sample,
                config,
                rendered.summary,
                sum,
                sum_squares);

            if (capture_samples) {
                for (uint32_t channel = 0; channel < channels; ++channel) {
                    rendered.interleaved_samples.push_back(sample);
                }
            }

            previous_sample = std::isfinite(sample) ? sample : 0.0f;
            has_previous_sample = true;
        }

        if (transport.current_row + 1 >= pattern.row_count()) {
            transport.finished = true;
            note_off(voice);
            break;
        }

        ++transport.current_row;
        transport.frames_until_row = transport.frames_per_row;
        apply_row_event(pattern, transport.current_row, voice, snapshot.frequency_hz, config.sample_rate);
        count_row_event(pattern, transport.current_row, rendered.summary);
    }

    if (rendered.summary.rendered_frames > 0) {
        rendered.summary.duration_seconds =
            static_cast<float>(rendered.summary.rendered_frames) / static_cast<float>(config.sample_rate);
        rendered.summary.dc_offset =
            static_cast<float>(sum / static_cast<double>(rendered.summary.rendered_frames));
        rendered.summary.rms =
            static_cast<float>(std::sqrt(sum_squares / static_cast<double>(rendered.summary.rendered_frames)));
    }

    return rendered;
}

void print_audio_debug_summary(
    std::ostream& out,
    const PatternSnapshot& snapshot,
    const AudioDebugConfig& config,
    const AudioDebugSummary& summary) {
    out << "audio debug summary\n";
    out << "  rows: " << snapshot.pattern.row_count() << "\n";
    out << "  render: " << summary.rendered_frames << " frames, " << std::fixed << std::setprecision(3)
        << summary.duration_seconds << " s, " << config.sample_rate << " Hz, " << config.channels
        << " ch\n";
    out << "  events: " << summary.note_on_events << " note-on, " << summary.note_off_events << " note-off, "
        << summary.empty_rows << " empty\n";
    out << "  levels: peak " << std::setprecision(5) << summary.peak_abs << ", rms " << summary.rms
        << ", dc " << summary.dc_offset << "\n";
    out << "  anomalies: " << summary.clipped_frames << " clipped, " << summary.non_finite_frames
        << " non-finite, " << summary.discontinuity_count << " discontinuities, "
        << summary.silent_frames << " silent frames\n";
    out << "  max delta: " << summary.max_delta << "\n";
    out << std::defaultfloat;
}

tl::expected<void, std::string> write_rendered_wav(
    const std::string& path,
    const RenderedAudio& rendered,
    const AudioDebugConfig& config) {
    if (config.channels == 0) {
        return tl::unexpected(std::string("wav export requires at least one channel"));
    }
    if (rendered.interleaved_samples.empty()) {
        return tl::unexpected(std::string("wav export requires captured samples"));
    }
    if (rendered.interleaved_samples.size() % config.channels != 0) {
        return tl::unexpected(std::string("captured sample count does not align with channel count"));
    }

    const uint64_t sample_count = rendered.interleaved_samples.size();
    const uint64_t data_bytes_64 = sample_count * sizeof(int16_t);
    if (data_bytes_64 > (std::numeric_limits<uint32_t>::max)()) {
        return tl::unexpected(std::string("wav export is too large for a RIFF/WAVE file"));
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return tl::unexpected(std::string("failed to open output wav file"));
    }

    const uint32_t data_bytes = static_cast<uint32_t>(data_bytes_64);
    const uint32_t riff_chunk_size = 36U + data_bytes;
    const uint16_t bits_per_sample = 16;
    const uint16_t block_align = static_cast<uint16_t>(config.channels * (bits_per_sample / 8U));
    const uint32_t byte_rate = config.sample_rate * block_align;

    out.write("RIFF", 4);
    write_u32(out, riff_chunk_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(out, 16U);
    write_u16(out, 1U);
    write_u16(out, static_cast<uint16_t>(config.channels));
    write_u32(out, config.sample_rate);
    write_u32(out, byte_rate);
    write_u16(out, block_align);
    write_u16(out, bits_per_sample);
    out.write("data", 4);
    write_u32(out, data_bytes);

    for (float sample : rendered.interleaved_samples) {
        if (!std::isfinite(sample)) {
            sample = 0.0f;
        }
        sample = (std::clamp)(sample, -1.0f, 1.0f);
        const auto pcm = static_cast<int16_t>(std::lround(sample * 32767.0f));
        write_u16(out, static_cast<uint16_t>(pcm));
    }

    if (!out) {
        return tl::unexpected(std::string("failed while writing wav file"));
    }

    return {};
}

}  // namespace gaga
