#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

#include <tl/expected.hpp>

#include "pattern.hpp"
#include "synth.hpp"
#include "transport.hpp"

struct ma_device;

namespace gaga {

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
    bool loop_enabled);

tl::expected<void, RuntimeErrorKind> start_audio_engine(AudioEngine& engine);
void stop_audio_engine(AudioEngine& engine);
bool pop_runtime_event(RuntimeEventQueue& queue, RuntimeEvent& event);
void store_pending_snapshot(AudioEngine& engine, std::shared_ptr<const PatternSnapshot> snapshot);

}  // namespace gaga
