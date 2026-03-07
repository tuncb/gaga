#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include <tl/expected.hpp>

#include "pattern.hpp"

namespace gaga {

struct AudioDebugConfig {
    uint32_t sample_rate = 48000;
    uint32_t channels = 2;
    int bpm = 120;
    int lpb = 4;
    float clip_threshold = 0.99f;
    float silence_threshold = 1.0e-4f;
    float discontinuity_threshold = 0.1f;
};

struct AudioDebugSummary {
    uint64_t rendered_frames = 0;
    uint64_t note_on_events = 0;
    uint64_t note_off_events = 0;
    uint64_t empty_rows = 0;
    uint64_t silent_frames = 0;
    uint64_t clipped_frames = 0;
    uint64_t non_finite_frames = 0;
    uint64_t discontinuity_count = 0;
    float duration_seconds = 0.0f;
    float peak_abs = 0.0f;
    float rms = 0.0f;
    float dc_offset = 0.0f;
    float max_delta = 0.0f;
};

struct RenderedAudio {
    std::vector<float> interleaved_samples;
    AudioDebugSummary summary;
};

RenderedAudio render_pattern_audio_debug(
    const PatternSnapshot& snapshot,
    const AudioDebugConfig& config,
    bool capture_samples);
void print_audio_debug_summary(
    std::ostream& out,
    const PatternSnapshot& snapshot,
    const AudioDebugConfig& config,
    const AudioDebugSummary& summary);
tl::expected<void, std::string> write_rendered_wav(
    const std::string& path,
    const RenderedAudio& rendered,
    const AudioDebugConfig& config);

}  // namespace gaga
