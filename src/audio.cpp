#include "audio.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <cstring>
#include <memory>

namespace gaga {

namespace {

void push_runtime_event(RuntimeEventQueue& queue, const RuntimeEvent& event) {
    const uint32_t write = queue.write_index.load(std::memory_order_relaxed);
    const uint32_t read = queue.read_index.load(std::memory_order_acquire);
    if (write - read >= queue.events.size()) {
        return;
    }

    queue.events[write % queue.events.size()] = event;
    queue.write_index.store(write + 1, std::memory_order_release);
}

void publish_display_state(AudioEngine& engine, const PatternSnapshot& snapshot, uint32_t row) {
    engine.display_state.store(
        encode_display_state(snapshot.display_generation, row),
        std::memory_order_release);
}

void render_frames(float* output, uint32_t frames, uint32_t channels, SynthVoice& voice, float master_gain) {
    for (uint32_t frame = 0; frame < frames; ++frame) {
        const float sample = next_sample(voice) * master_gain;
        for (uint32_t channel = 0; channel < channels; ++channel) {
            output[frame * channels + channel] = sample;
        }
    }
}

void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)input;

    auto* engine = static_cast<AudioEngine*>(device->pUserData);
    auto* out = static_cast<float*>(output);
    if (engine == nullptr || engine->active_snapshot == nullptr) {
        std::memset(output, 0, static_cast<size_t>(frame_count) * sizeof(float) * device->playback.channels);
        return;
    }

    uint32_t remaining_frames = frame_count;
    while (remaining_frames > 0) {
        if (engine->transport.finished) {
            std::fill(out, out + static_cast<size_t>(remaining_frames) * device->playback.channels, 0.0f);
            note_off(engine->voice);
            engine->playback_finished.store(true, std::memory_order_release);
            return;
        }

        const auto& pattern = engine->active_snapshot->pattern;
        if (pattern.row_count() == 0 || engine->transport.current_row >= pattern.row_count()) {
            push_runtime_event(
                engine->runtime_events,
                RuntimeEvent{RuntimeSeverity::Fatal, RuntimeErrorKind::InternalStateError, 0, 0});
            engine->shutdown_requested.store(true, std::memory_order_release);
            std::fill(out, out + static_cast<size_t>(remaining_frames) * device->playback.channels, 0.0f);
            return;
        }

        const uint32_t chunk = (std::min)(remaining_frames, engine->transport.frames_until_row);
        render_frames(out, chunk, device->playback.channels, engine->voice, engine->master_gain);
        out += static_cast<size_t>(chunk) * device->playback.channels;
        remaining_frames -= chunk;
        engine->transport.frames_until_row -= chunk;

        if (engine->transport.frames_until_row > 0) {
            continue;
        }

        if (engine->transport.current_row + 1 >= pattern.row_count()) {
            if (!engine->transport.loop_enabled) {
                engine->transport.finished = true;
                note_off(engine->voice);
                continue;
            }

            auto pending = std::atomic_exchange_explicit(
                &engine->pending_snapshot,
                std::shared_ptr<const PatternSnapshot>{},
                std::memory_order_acq_rel);
            if (pending) {
                engine->active_snapshot = std::move(pending);
            }

            engine->transport.current_row = 0;
            if (engine->active_snapshot->pattern.row_count() == 0) {
                publish_display_state(*engine, *engine->active_snapshot, kNoDisplayedRow);
                note_off(engine->voice);
                continue;
            }

            apply_row_event(
                engine->active_snapshot->pattern,
                0,
                engine->transport,
                engine->voice,
                engine->master_gain,
                engine->synth_type,
                engine->active_snapshot->frequency_hz,
                engine->sample_rate);
            engine->transport.frames_until_row = engine->transport.frames_per_row;
            publish_display_state(*engine, *engine->active_snapshot, 0);
            continue;
        }

        ++engine->transport.current_row;
        apply_row_event(
            pattern,
            engine->transport.current_row,
            engine->transport,
            engine->voice,
            engine->master_gain,
            engine->synth_type,
            engine->active_snapshot->frequency_hz,
            engine->sample_rate);
        engine->transport.frames_until_row = engine->transport.frames_per_row;
        publish_display_state(*engine, *engine->active_snapshot, engine->transport.current_row);
    }
}

}  // namespace

tl::expected<void, RuntimeErrorKind> initialize_audio_engine(
    AudioEngine& engine,
    std::shared_ptr<const PatternSnapshot> active_snapshot,
    int bpm,
    int lpb,
    bool loop_enabled,
    SynthType synth_type) {
    engine.active_snapshot = std::move(active_snapshot);
    engine.master_gain = 1.0f;
    engine.synth_type = synth_type;
    initialize_transport(
        engine.transport,
        engine.sample_rate,
        bpm,
        lpb,
        loop_enabled,
        engine.active_snapshot != nullptr && engine.active_snapshot->pattern.row_count() > 0);

    if (engine.active_snapshot == nullptr) {
        engine.display_state.store(encode_display_state(0, kNoDisplayedRow), std::memory_order_release);
    } else if (engine.active_snapshot->pattern.row_count() > 0) {
        apply_row_event(
            engine.active_snapshot->pattern,
            0,
            engine.transport,
            engine.voice,
            engine.master_gain,
            engine.synth_type,
            engine.active_snapshot->frequency_hz,
            engine.sample_rate);
        engine.transport.frames_until_row = engine.transport.frames_per_row;
        publish_display_state(engine, *engine.active_snapshot, 0);
    } else {
        publish_display_state(engine, *engine.active_snapshot, kNoDisplayedRow);
    }

    auto* device = new ma_device{};
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = engine.channels;
    config.sampleRate = engine.sample_rate;
    config.dataCallback = audio_callback;
    config.pUserData = &engine;

    if (ma_device_init(nullptr, &config, device) != MA_SUCCESS) {
        delete device;
        return tl::unexpected(RuntimeErrorKind::AudioInitFailed);
    }

    engine.device_handle = device;
    return {};
}

tl::expected<void, RuntimeErrorKind> start_audio_engine(AudioEngine& engine) {
    if (engine.device_handle == nullptr) {
        return tl::unexpected(RuntimeErrorKind::AudioInitFailed);
    }

    if (ma_device_start(engine.device_handle) != MA_SUCCESS) {
        return tl::unexpected(RuntimeErrorKind::AudioStartFailed);
    }

    return {};
}

void stop_audio_engine(AudioEngine& engine) {
    if (engine.device_handle != nullptr) {
        ma_device_stop(engine.device_handle);
        ma_device_uninit(engine.device_handle);
        delete engine.device_handle;
        engine.device_handle = nullptr;
    }
}

bool pop_runtime_event(RuntimeEventQueue& queue, RuntimeEvent& event) {
    const uint32_t read = queue.read_index.load(std::memory_order_relaxed);
    const uint32_t write = queue.write_index.load(std::memory_order_acquire);
    if (read == write) {
        return false;
    }

    event = queue.events[read % queue.events.size()];
    queue.read_index.store(read + 1, std::memory_order_release);
    return true;
}

void store_pending_snapshot(AudioEngine& engine, std::shared_ptr<const PatternSnapshot> snapshot) {
    std::atomic_store_explicit(&engine.pending_snapshot, std::move(snapshot), std::memory_order_release);
}

}  // namespace gaga
