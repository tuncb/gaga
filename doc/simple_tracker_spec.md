# Simple Tracker Console App Specification

## Overview

A minimal tracker-style console application named `gaga` that reads note
rows from a `.gaga` text file, parses them into events, and plays them using a simple
oscillator via the miniaudio library.

Language: **C++20**\
Audio Library: **miniaudio**
Build System: **CMake**

The goal is to mimic **basic tracker note semantics** similar to devices
like the M8, while keeping the implementation extremely simple.

------------------------------------------------------------------------

# Goals

The application should:

-   Read a `.gaga` text pattern file
-   Parse tracker-style note tokens
-   Maintain note state across rows
-   Play notes using a simple synthesized oscillator
-   Support optional looped playback with reload-on-loop behavior
-   Provide helpful parsing errors
-   Run entirely in the console

------------------------------------------------------------------------

# Non‑Goals (Version 1)

Not included in the first version:

-   Multiple tracks
-   Instruments
-   Effects
-   Sample playback
-   Pattern chaining
-   Song mode
-   MIDI input/output
-   Swing or groove
-   Advanced envelopes

------------------------------------------------------------------------

# File Format

Each line represents a **row**.

Recommended file extension:

    .gaga

Allowed tokens:

  Token            Meaning
  ---------------- ---------------------------
  `C-0` to `B-9`   Start or retrigger a note
  `---`            No event
  `OFF`            Stop the current note

Example:

    C-4
    ---
    E-4
    ---
    G-4
    OFF
    ---
    C-5

------------------------------------------------------------------------

# Token Semantics

### Note Token

Starts or retriggers a note.

Examples:

    C-4
    D#5
    A-2

### Empty Row (`---`)

Means **no event occurs**.

If a note is currently active it continues playing.

### OFF Token

Explicitly stops the current note.

If no note is active, it does nothing.

------------------------------------------------------------------------

# Playback Model

The sequencer is **monophonic**.

Note state at any moment:

-   `Silent`
-   `Playing(note)`

Transport state:

-   `Running`
-   `Finished`

### State Transition Table

  Current State   Token      Next State
  --------------- ---------- ---------------
  Silent          `---`      Silent
  Silent          `OFF`      Silent
  Silent          `NOTE`     Playing(note)
  Playing(X)      `---`      Playing(X)
  Playing(X)      `OFF`      Silent
  Playing(X)      `NOTE Y`   Playing(Y)

Important rule:

`---` does **not mean silence**.\
It means **no change to current state**.

------------------------------------------------------------------------

# Timing Model

Tracker timing is based on:

-   **BPM** (beats per minute)
-   **LPB** (lines per beat)

Defaults:

    BPM = 120
    LPB = 4

Row duration:

    seconds_per_row = 60 / (BPM * LPB)

Example:

    60 / (120 * 4) = 0.125 seconds

------------------------------------------------------------------------

# Audio Model

Audio output is generated using a **simple oscillator**.

Voice properties:

    active: bool
    phase: float
    phase_step: float

Waveform (v1):

-   sine wave

------------------------------------------------------------------------

# Frequency Calculation

Convert note to MIDI number:

    midi = (octave + 1) * 12 + pitchClass

Frequency:

    frequency = 440 * 2^((midi - 69) / 12)

------------------------------------------------------------------------

# Note Parsing

The parser must use a **real tokenizer and parser**.

Requirements:

-   no regular expressions
-   no `std::regex`
-   parse from a source buffer using explicit character scanning
-   tokenize first, then parse tokens into row operations
-   keep source location information for diagnostics

Valid note token shape:

    [A-G][#|-][0-9]

Supported note range:

    C-0 through B-9

The parser should validate both:

-   token syntax
-   supported note range

Suggested grammar:

    file := line* EOF
    line := ws* (row ws* comment? | comment)? ws* (newline | EOF)
    row := note | OFF | ---
    note := [A-G] ('#' | '-') [0-9]

Tokenizer responsibilities:

-   scan the source text once from left to right
-   recognize note letters, `#`, `-`, digits, `OFF`, `---`, comments, newlines, and end-of-file
-   skip spaces and tabs while still maintaining correct line and column tracking
-   attach 1-based line and column information to each token
-   emit invalid tokens when encountering unexpected characters

Parser responsibilities:

-   consume tokenizer output row by row
-   reject extra trailing tokens on a row
-   compile valid rows into a compact runtime pattern representation
-   produce structured diagnostics with source line and column information

Examples:

Valid:

    C-4
    F#3
    A-0

Invalid:

    Db4
    C4
    H-4
    C#10

------------------------------------------------------------------------

# Comments and Blank Lines

Allowed:

    # this is a comment
    C-4
    C-4 # start note
    ---

    OFF
    OFF # we stop the note here

Rules:

-   blank lines ignored
-   lines beginning with `#` ignored
-   trailing comments after a valid row token are allowed
-   ignored lines do not create pattern rows
-   comments extend from `#` to the end of the physical source line
-   text after `#` is ignored by the parser
-   the final line may omit a trailing newline
-   parse errors report the original 1-based source line number and column

------------------------------------------------------------------------

# Program Usage

Example:

    gaga filename.gaga

Optional flags:

    gaga filename.gaga --loop
    gaga filename.gaga --bpm 120 --lpb 4
    gaga filename.gaga --bpm 120 --lpb 4 --loop

Default behavior:

-   parse the input file
-   print normalized rows
-   start playback immediately after a successful parse
-   if startup parsing fails, do not start playback and exit with a non-zero status

Loop mode:

-   `--loop` restarts playback from row `0` after the final row duration completes
-   while running, the program watches the source file for changes
-   if the file changes, the main thread reloads and reparses it into a pending pattern snapshot
-   reload happens only at the loop boundary, never in the middle of a row
-   at the next loop boundary, the program activates the pending snapshot if one is available
-   if reload fails, the program reports the error and continues looping the last valid pattern

------------------------------------------------------------------------

# Console Output

After parsing the file, the program should print normalized rows:

    000 C-4
    001 ---
    002 E-4
    003 ---
    004 G-4
    005 OFF
    006 ---
    007 C-5

Normalized row numbers are:

-   zero-based
-   based on parsed pattern rows after comments and blank lines are removed

Optional trace mode:

    row 000: note on C-4 (261.63 Hz)
    row 005: note off
    row 007: note on C-5 (523.25 Hz)

Error output examples:

    error: filename.gaga:12:5: unexpected token
    warning: reload failed, continuing with previous pattern
    fatal: audio device lost

------------------------------------------------------------------------

# Error Handling

Error classes:

-   startup parse errors
-   startup file I/O errors
-   audio device initialization or start errors
-   recoverable loop reload errors
-   fatal runtime playback errors

Startup policy:

-   the initial file load, tokenization, and parse must complete before audio device start
-   if the initial file cannot be opened or read, print an error and exit with code `1`
-   if the initial file contains tokenizer or parser errors, print diagnostics and exit with code `1`
-   if audio device initialization or start fails, print a fatal runtime error and exit with code `2`

Loop reload policy:

-   reload failures in `--loop` mode are warnings, not fatal startup errors
-   if a changed file cannot be opened, read, tokenized, or parsed, keep the current active pattern
-   the current loop pass is never interrupted by a reload failure
-   print at most one warning per failed file version
-   retry reload only after file metadata changes again

Fatal runtime policy:

-   unrecoverable playback errors must request shutdown
-   shutdown should silence the voice, stop audio cleanly, and exit with code `3`
-   examples include unrecoverable device loss, audio backend failure, or corrupted active playback state

Diagnostic formatting:

-   syntax and parse diagnostics use `error: path:line:column: message`
-   recoverable runtime issues use `warning: message`
-   fatal runtime issues use `fatal: message`
-   if practical, syntax diagnostics should also print the source line and a caret marker

Reporting rules:

-   expected tokenizer, parser, file I/O, and reload failures should use `tl::expected`, not exceptions for control flow
-   repeated warnings should be deduplicated to avoid console spam
-   the main thread is responsible for formatting and printing errors

Exit codes:

-   `0` normal exit
-   `1` startup file or parse failure
-   `2` audio initialization or start failure
-   `3` fatal runtime playback failure

------------------------------------------------------------------------

# C++ Architecture

Project name:

    gaga

Build system:

    CMake

Suggested modules:

    CMakeLists.txt
    source_text.hpp / source_text.cpp
    tokenizer.hpp / tokenizer.cpp
    parser.hpp / parser.cpp
    note.hpp / note.cpp
    pattern.hpp / pattern.cpp
    transport.hpp / transport.cpp
    synth.hpp / synth.cpp
    audio.hpp / audio.cpp
    file_watch.hpp / file_watch.cpp
    main.cpp

CMake requirements:

-   require C++20
-   produce an executable named `gaga`
-   include miniaudio in the build
-   include `tl::expected` and `tl::optional` in the build

Implementation style:

-   data-oriented design
-   no inheritance
-   no virtual dispatch
-   no regex-driven parsing
-   prefer plain structs and free functions over stateful classes
-   use `tl::expected<T, E>` for fallible operations
-   use `tl::optional<T>` where a nullable value is genuinely needed

Module responsibilities:

-   `source_text`: load file bytes and retain source text for diagnostics
-   `tokenizer`: convert source bytes into a token stream
-   `parser`: validate token sequences and compile them into pattern data
-   `note`: pitch-class mapping and note-index conversion helpers
-   `pattern`: compiled pattern storage
-   `transport`: row timing, loop boundaries, and pattern position
-   `synth`: oscillator voice state and sample generation
-   `audio`: miniaudio device glue and callback rendering
-   `file_watch`: detect timestamp or size changes for loop reload

------------------------------------------------------------------------

# Core Data Structures

The runtime should favor compact, contiguous arrays and simple scalar
types. The audio thread should consume compiled pattern data, not parse
text or traverse object graphs.

## Source Text

    struct SourceText {
        std::string path;
        std::vector<char> bytes;
    };

## Token Stream

    enum class TokenKind : uint8_t {
        NoteLetter,
        Sharp,
        Dash,
        Digit,
        Off,
        TripleDash,
        Comment,
        Newline,
        EndOfFile,
        Invalid
    };

    struct TokenStream {
        std::vector<TokenKind> kind;
        std::vector<uint32_t> offset;
        std::vector<uint16_t> length;
        std::vector<uint32_t> line;
        std::vector<uint16_t> column;
    };

## Diagnostics

    enum class DiagnosticKind : uint8_t {
        InvalidCharacter,
        InvalidToken,
        UnexpectedToken,
        TrailingTokens,
        NoteOutOfRange
    };

    struct Diagnostic {
        DiagnosticKind kind;
        uint32_t line;
        uint16_t column;
        uint32_t offset;
        uint16_t length;
    };

## Runtime Events

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
        std::array<RuntimeEvent, 64> events;
        std::atomic<uint32_t> write_index;
        std::atomic<uint32_t> read_index;
    };

## Note Data

    enum class PitchClass {
        C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B
    };

    struct Note {
        PitchClass pitch;
        int octave;
    };

## Compiled Pattern

    enum class RowOp : uint8_t {
        Empty,
        NoteOn,
        NoteOff
    };

    struct PatternData {
        std::vector<RowOp> op;
        std::vector<uint8_t> note_index;
        std::vector<uint32_t> source_line;
    };

Notes:

-   `note_index` is a zero-based semitone index where `C-0 = 0` and `B-9 = 119`
-   `note_index` is only read when `op[row] == RowOp::NoteOn`
-   `frequency_hz[note_index]` stores the precomputed playback frequency for that note
-   `source_line[row]` maps compiled rows back to the original file for diagnostics or trace output

## Pattern Snapshot

    struct PatternSnapshot {
        PatternData pattern;
        std::vector<float> frequency_hz;
        uint64_t file_timestamp;
        uint64_t file_size;
    };

## Transport State

    struct TransportState {
        uint32_t current_row;
        uint32_t frames_until_row;
        uint32_t frames_per_row;
        bool loop_enabled;
        bool finished;
    };

------------------------------------------------------------------------

# Synth Voice

    struct SynthVoice {
        bool active;
        float phase;
        float phase_step;
    };

Behavior:

-   `noteOn(freq)` activates voice, resets phase to `0.0`, and updates `phase_step`
-   `noteOff()` stops voice
-   `nextSample()` generates waveform sample
-   the hot path should use `phase_step` directly instead of recalculating from frequency every sample

------------------------------------------------------------------------

# Sequencer

Responsibilities:

-   track current row
-   advance row timing in sample frames
-   apply row events from compiled pattern data
-   control synth voice
-   coordinate loop boundaries with pattern reload

There should be no inheritance-based sequencer object model. Prefer a
small transport state struct plus free functions.

Suggested free-function API:

    void apply_row_event(
        const PatternData& pattern,
        uint32_t row,
        SynthVoice& voice,
        std::span<const float> frequency_hz);

    void advance_transport(
        TransportState& transport,
        uint32_t rendered_frames);

    bool handle_loop_boundary(
        TransportState& transport,
        const PatternSnapshot*& active_pattern,
        const PatternSnapshot* pending_pattern);

Requirements:

-   transport timing should be frame-based, not driven by a floating-point seconds accumulator
-   the renderer must handle zero, one, or multiple row transitions in a single audio callback
-   row events take effect exactly at the row boundary
-   when playback passes the end of the final row:
    -   without `--loop`, transport enters a finished state
    -   with `--loop`, transport performs a loop boundary check and restarts from row `0`
-   if loop-mode reload succeeds, the next pass uses the new pattern from row `0`
-   if loop-mode reload fails, the previous valid pattern remains active

------------------------------------------------------------------------

# Audio System

Uses **miniaudio**.

Device settings:

    sample_rate = 48000
    channels = 2
    format = f32

Audio callback:

1.  determine how many sample frames remain until the next row boundary
2.  render audio up to that boundary
3.  apply the row event exactly on the boundary
4.  if the boundary is the end of the pattern and `--loop` is enabled, swap in a pending pre-parsed pattern if one is available
5.  repeat as needed until the output buffer is filled

Mono oscillator output duplicated to stereo.

The callback may cross multiple row boundaries in one buffer, so the
sequencer must split rendering accordingly.

Audio-thread safety rules:

-   the audio callback must not read files
-   the audio callback must not tokenize or parse text
-   the audio callback must not allocate memory for normal playback
-   the audio callback must not print to the console
-   the audio callback must not throw exceptions for expected runtime failures
-   file change detection, file loading, tokenization, and parsing happen off the audio thread
-   loop-boundary activation uses already prepared pattern snapshots
-   runtime warnings or fatal conditions detected on the audio thread should be written into a preallocated runtime event queue
-   the main thread drains the runtime event queue and prints messages

Loop reload model:

-   main thread watches the source file while playback is running in `--loop` mode
-   when the file changes, the main thread builds a new `PatternSnapshot`
-   the new snapshot becomes `pending`
-   at the next loop boundary, audio switches from `active` to `pending`
-   if snapshot build fails, `active` remains unchanged and a warning is printed once for that file version

------------------------------------------------------------------------

# Edge Case Rules

  Case                     Behavior
  ------------------------ -------------------
  `OFF` while silent       no effect
  repeated note            retrigger and reset phase
  new note while playing   replaces previous and reset phase
  empty row                preserve state
  blank lines              ignored
  comments                 ignored
  end of pattern           stop playback and silence voice when `--loop` is off
  end of pattern in loop   swap in pending snapshot if available, then restart at row 0
  reload parse failure     keep last valid pattern and continue looping
  startup parse failure    print diagnostics, do not start playback, exit code 1
  audio init failure       print fatal error, do not start playback, exit code 2
  fatal runtime failure    stop playback cleanly and exit code 3

------------------------------------------------------------------------

# Known Limitations

This minimal format has limitations:

-   no instruments
-   no effects
-   no velocity
-   no single-row rest token distinct from `OFF` + `---`
-   no metadata in file

Note length is determined by the distance between:

-   note start
-   `OFF`
-   next note
-   end of pattern

------------------------------------------------------------------------

# Implementation Milestones

### Milestone 1

Project scaffold and parser.

-   CMake project
-   read file
-   parse rows
-   print normalized pattern

### Milestone 2

Playback.

-   oscillator
-   row timing
-   note events
-   default `gaga filename.gaga` playback flow

### Milestone 3

Polish.

-   CLI flags
-   `--loop` live reload at loop boundary
-   error reporting
-   runtime warning and fatal-event handling
-   unit tests

------------------------------------------------------------------------

# Design Principle

Tracker rows represent **events**, not durations.

Rows only say:

-   start a note
-   stop a note
-   do nothing

The **duration** of a note is the time between these events.
