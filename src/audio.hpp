#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>

#include <tl/expected.hpp>

#include "pattern.hpp"
#include "synth.hpp"
#include "transport.hpp"

struct ma_device;

namespace gaga {

constexpr uint32_t kNoDisplayedRow = (std::numeric_limits<uint32_t>::max)();

constexpr uint64_t encode_display_state(uint32_t display_generation, uint32_t row) {
    return (static_cast<uint64_t>(display_generation) << 32U) | static_cast<uint64_t>(row);
}

constexpr uint32_t decode_display_generation(uint64_t display_state) {
    return static_cast<uint32_t>(display_state >> 32U);
}

constexpr uint32_t decode_display_row(uint64_t display_state) {
    return static_cast<uint32_t>(display_state & 0xffffffffULL);
}

enum class RuntimeSeverity : uint8_t {
    Warning,
    Fatal
};

enum class RuntimeErrorKind : uint8_t {
    FileOpenFailed,
    FileReadFailed,
    FileWatchFailed,
    AudioInitFailed,
    AudioStartFailed,
    AudioDeviceLost,
    ReloadFailed,
    InternalStateError
};

struct RuntimeEvent {
    RuntimeSeverity severity;
    RuntimeErrorKind kind;
    uint32_t line;
    uint16_t column;
};

struct RuntimeEventQueue {
    std::array<RuntimeEvent, 64> events{};
    std::atomic<uint32_t> write_index{0};
    std::atomic<uint32_t> read_index{0};
};

struct AudioEngine {
    ma_device* device_handle = nullptr;
    std::shared_ptr<const PatternSnapshot> active_snapshot;
    std::shared_ptr<const PatternSnapshot> pending_snapshot;
    RuntimeEventQueue runtime_events;
    TransportState transport;
    SynthVoice voice;
    SynthType synth_type = SynthType::Square;
    std::atomic<uint64_t> display_state{encode_display_state(0, kNoDisplayedRow)};
    std::atomic<bool> playback_finished{false};
    std::atomic<bool> shutdown_requested{false};
    uint32_t sample_rate = 48000;
    uint32_t channels = 2;
};

tl::expected<void, RuntimeErrorKind> initialize_audio_engine(
    AudioEngine& engine,
    std::shared_ptr<const PatternSnapshot> active_snapshot,
    int bpm,
    int lpb,
    bool loop_enabled,
    SynthType synth_type);

tl::expected<void, RuntimeErrorKind> start_audio_engine(AudioEngine& engine);
void stop_audio_engine(AudioEngine& engine);
bool pop_runtime_event(RuntimeEventQueue& queue, RuntimeEvent& event);
void store_pending_snapshot(AudioEngine& engine, std::shared_ptr<const PatternSnapshot> snapshot);

}  // namespace gaga
