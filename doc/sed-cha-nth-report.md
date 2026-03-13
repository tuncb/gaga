# SED / CHA / NTH Implementation Report

Date: 2026-03-10

## Executive Summary

`gaga` already has the row-FX infrastructure that older gap notes said was missing. The parser accepts a row event plus any number of `FX value` pairs, the compiled pattern stores flat per-row FX lists, and the transport applies them at row boundaries.

That changes the scope of `SED`, `CHA`, and `NTH`:

- `SED` is mostly a runtime-state feature.
- `CHA` and `NTH` are mostly a semantics problem.
- Exact M8 phrase semantics are not available with the current row model.

Recommended direction:

1. Implement all three as simplified row-wide commands, not exact M8 left/right column commands.
2. Add a small per-track runtime-state object for deterministic RNG and loop-pass counting.
3. Refactor row execution into a guard/prepass stage plus a normal execution stage.

If we want exact M8 behavior later, that should be treated as a larger sequencer model change, not as part of this feature.

## What Exists Today

The current implementation is more advanced than the older design notes imply.

### Parser and pattern model

- `src/parser.cpp` already parses a row event followed by zero or more `WORD HEXBYTE` FX pairs.
- `src/pattern.hpp` already stores row FX as flat arrays with `fx_start` and `fx_count`.
- `src/pattern.cpp` already formats row FX back to text for tracing and display.
- `src/tokenizer.cpp` already tokenizes generic uppercase words and hex bytes, so no tokenizer change is required for these commands.

Current supported FX are:

- `VOL`
- `PIT`
- `FIN`
- `TSP`
- `TPO`
- `VMV`

### Runtime behavior

- `src/transport.cpp` applies the row event first, then iterates the row FX list.
- `TPO` changes `frames_per_row` before the caller resets `frames_until_row`, so it affects the current row duration.
- `VOL`, `PIT`, `FIN`, `TSP`, and `VMV` all persist until another row changes them.
- `src/audio.cpp` swaps in a reloaded pattern only at the loop boundary in `--loop` mode.
- On loop wrap or hot reload, the playhead resets to row `0`, but transport tempo, synth state, and master gain are preserved.

### Tests already in place

- `tests/parser_tests.cpp` covers row FX parsing and rejection of unknown commands.
- `tests/audio_debug_tests.cpp` covers row-FX application for the currently supported commands.

## Why SED / CHA / NTH Are Different

`SED`, `CHA`, and `NTH` do not need sub-row scheduling, DSP work, or a new parser format. They do need state and execution semantics that the current runtime does not have.

### SED

`SED` needs a deterministic RNG state attached to playback, not to the parsed pattern.

### CHA

The M8 manual describes `CHA` in terms of chance applied to content on the left or right of the command. `gaga` does not have phrase columns or ordered row cells. It has:

- one row event stored outside the FX list
- a flat list of FX values stored after that row event

Because of that, exact left/right semantics cannot be implemented without changing the row model.

### NTH

The same problem applies to `NTH`: M8 describes it as skipping content on one side of the command based on loop count. `gaga` currently has no notion of directional row content beyond "row event first, then all FX".

## Decisions We Need To Make

### 1. Scope: exact M8 semantics or simplified `gaga` semantics

Two viable choices exist.

#### Option A: exact M8-style left/right semantics

Pros:

- closer to M8 behavior
- better long-term compatibility if the app grows toward phrase columns

Cons:

- requires a larger row representation change
- `apply_row_event()` cannot stay "row event first, then FX"
- likely needs the row event represented as an ordered cell in the same stream as FX
- makes this feature much larger than it currently looks

#### Option B: simplified row-wide semantics

Pros:

- small feature with clear implementation boundaries
- fits the current one-track row model
- does not require UI or syntax redesign

Cons:

- not exact M8 behavior
- future migration to exact compatibility would be a behavior change

Recommendation: choose Option B now.

### 2. `CHA` semantics

Recommended behavior:

- `CHA XY` is a row-wide probability gate.
- Evaluate it once at row entry, after any same-row `SED`, and before the row event and non-guard FX on that row.
- If the gate fails, skip the entire row:
  - no note-on
  - no note-off
  - no `VOL`/`PIT`/`FIN`/`TSP`/`TPO`/`VMV`

Reason:

- this is the only behavior that is simple, deterministic, and meaningful on `---` rows
- it avoids fake left/right rules that the current syntax does not express

Recommended value mapping:

- treat the stored byte as a full-byte probability threshold
- `00` = never
- `FF` = always
- anything in between uses `rng_byte < value`

This is a deliberate simplification from M8, but it matches the current parser and gives useful resolution.

### 3. `NTH` semantics

Recommended behavior:

- `NTH XY` is a row-wide loop-pass guard.
- Evaluate it after any same-row `SED`, before the row event, and before any non-guard FX on the row.
- If the condition fails, skip the entire row.

Recommended pass-count model:

- playback starts on pass `1`
- increment the pass counter when the pattern wraps from the last row back to row `0`
- hot reload in `--loop` mode does not reset the counter

Recommended value mapping:

- high nibble `X` = period, using `max(1, X)`
- low nibble `Y` = phase
- row executes when `((loop_pass - 1) % period) == (Y % period)`

Examples:

- `NTH 10`: every pass
- `NTH 20`: passes 1, 3, 5, ...
- `NTH 21`: passes 2, 4, 6, ...
- `NTH 31`: passes 2, 5, 8, ...

Reason:

- it is simple to explain
- it uses both nibbles naturally
- it gives more control than treating the byte as a single exact pass number

This is a product choice, not a recovered M8 encoding. If exact M8 encoding is required, we should defer `NTH` until the actual on-device behavior is verified and we are willing to support a richer row model.

### 4. `SED` semantics

Recommended behavior:

- `SED XX` sets the current track RNG state immediately.
- It does not trigger randomness by itself.
- It affects any same-row guard evaluation and later rows.

Recommended state model:

- expand the seed byte into a deterministic 32-bit non-zero RNG state
- keep the RNG state in playback runtime state, not in `PatternSnapshot`
- `00` is legal and maps to a fixed non-zero state

Recommended persistence:

- preserve RNG state across rows
- preserve RNG state across loop wraps
- preserve RNG state across hot reloads
- reset RNG state only when playback restarts or a later `SED` row runs

Reason:

- this matches current engine behavior, which already preserves tempo, pitch state, and master gain across loop wraps and hot reloads
- making reload reset only RNG and `NTH` would be inconsistent

### 5. Hot reload behavior

This needs to be an explicit decision because `--loop` is already stateful.

Recommended rule:

- reloading a file at the loop boundary swaps the pattern only
- runtime state stays alive: tempo, pitch offsets, transpose, master gain, RNG state, and loop pass

If we want "reload means restart from clean initial state", that should be a separate broader change and should also address the already-supported stateful commands.

## Required Code Changes For The Recommended Path

### 1. Extend the FX enum and string mapping

Files:

- `src/pattern.hpp`
- `src/pattern.cpp`
- `src/parser.cpp`

Changes:

- add `FxCommand::Seed`
- add `FxCommand::Chance`
- add `FxCommand::Nth`
- map them to `SED`, `CHA`, and `NTH` in parser/formatter code

Complexity: small

### 2. Add track runtime state

Files:

- `src/transport.hpp`
- `src/audio.hpp`
- `src/audio.cpp`
- `src/audio_debug.cpp`

Recommended new struct:

```cpp
struct TrackRuntimeState {
    uint32_t loop_pass = 1;
    uint32_t rng_state = 0x12345678U;
};
```

Changes:

- store this state in `AudioEngine`
- create the same state in offline debug rendering
- pass it into row evaluation

Complexity: small to medium

### 3. Refactor row execution into prepass plus execution

Files:

- `src/transport.cpp`
- `src/transport.hpp`

Why this is needed:

- current execution order is fixed as row event first, then all FX
- `CHA` and `NTH` must decide whether the row executes before any row side effect happens

Recommended shape:

- first pass over row FX:
  - apply `SED`
  - evaluate `NTH`
  - evaluate `CHA`
  - decide whether the row is skipped
- second pass:
  - if not skipped, execute row event
  - then execute non-guard FX

Important detail:

- do not make `SED` conditional on guard success
- in the recommended row-wide model, all guard commands are interpreted as row guards, not positional commands

Complexity: medium

### 4. Track loop-pass progression

Files:

- `src/audio.cpp`
- `src/audio_debug.cpp`

Changes:

- increment `loop_pass` when playback wraps from the last row to row `0`
- preserve it across pending snapshot swaps in `--loop` mode

Complexity: small

### 5. Add deterministic RNG helpers

Files:

- likely `src/transport.cpp` or a new small helper file

Changes:

- seed-expansion helper from `uint8_t` to `uint32_t`
- one byte-at-a-time RNG helper for `CHA`
- guarantee non-zero state for xorshift-style generators

Complexity: small

### 6. Add tests

Files:

- `tests/parser_tests.cpp`
- `tests/audio_debug_tests.cpp`

Recommended parser coverage:

- `SED`, `CHA`, and `NTH` parse successfully
- unknown commands still fail

Recommended runtime coverage:

- `SED` produces deterministic `CHA` decisions
- `CHA 00` always skips
- `CHA FF` always executes
- `NTH 20` and `NTH 21` alternate across loop passes
- loop counter survives wrap
- loop counter and RNG survive hot-reload snapshot swap

Complexity: medium

### 7. Update docs and examples

Files:

- `README.md`
- `examples/README.md`
- new example patterns

Changes:

- document the simplified row-wide semantics explicitly
- avoid implying exact M8 compatibility
- add one deterministic `SED` + `CHA` example
- add one `NTH` loop example

Complexity: small

## What We Do Not Need For The Recommended Path

We do not need:

- tokenizer changes
- sub-row timing
- a scheduler
- synth DSP changes
- terminal UI redesign
- multiple tracks
- instrument tables

## What Would Be Required For Exact M8 Semantics

If exact M8-style left/right behavior is a hard requirement, the implementation scope is materially larger.

Required redesign:

- represent row content as an ordered stream, not as `row_event + flat fx list`
- make the row event an addressable item in that stream
- define exact positional execution rules
- update trace/display code to show the new structure clearly
- likely update syntax if we want something users can reason about

In practice this means `SED` can still ship cheaply, but `CHA` and `NTH` should be deferred until that redesign exists.

## Recommended Delivery Plan

1. Ship simplified row-wide `SED`, `CHA`, and `NTH`.
2. Document them as `gaga` commands inspired by M8, not exact M8 reproductions.
3. Revisit exact compatibility only if the project is already moving toward multi-column phrase semantics.

## Notes On Existing Docs

`doc/m8-command-gap-report.md` and `doc/next-set-of-commands.md` are now partially stale. They still describe the project as lacking row-FX parsing/storage, but that infrastructure already exists in the current codebase.
