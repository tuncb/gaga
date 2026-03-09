# M8 Tracker Command Gap Report

Date: 2026-03-09

## Executive Summary

The current `gaga` app does not support any of the M8 command examples in [m8-tracker-commands.md](m8-tracker-commands.md). That is expected: the app was intentionally scoped as a minimal v1 tracker with a monophonic note sequencer, no instruments, no effects, no multi-track playback, no song mode, and no groove support.

The best-fit commands to add first, without changing the overall product shape, are:

- `VOL`, `PIT`, `FIN`
- `TPO`, `TSP`
- `CHA`, `NTH`, `SED`
- `OFF XX`, `KIL`, `DEL`, `RET`, `MTT`
- `PSL`, `PBN`, `PVB`, `PVX`
- `ARP`, `ARC`
- `VMV` as an optional simple master-gain command

Everything else is blocked by at least one missing subsystem:

- multi-track / phrase / song sequencing
- instruments and instrument tables
- mixer buses, sends, delay, reverb, EQ, DJ filter, or audio inputs
- envelope and LFO modulation layers

## Evidence From The Current App

The current implementation is much narrower than the M8 command set:

- The spec explicitly excludes multiple tracks, instruments, effects, pattern chaining, song mode, groove, and advanced envelopes: `doc/simple_tracker_spec.md:32-44`.
- The file format only allows one row token per line: note, `OFF`, or `---`: `doc/simple_tracker_spec.md:56-62`, `doc/simple_tracker_spec.md:210-215`.
- The tokenizer only recognizes note pieces, `OFF`, `---`, comments, newlines, and invalid characters; it does not tokenize generic 3-letter FX mnemonics: `src/tokenizer.cpp:68-153`.
- The parser only emits three runtime operations and rejects trailing tokens on a row, so rows cannot currently contain note + FX columns: `src/parser.cpp:75-135`.
- The compiled pattern only stores `RowOp`, `note_index`, and `source_line`: `src/pattern.hpp:10-30`.
- The transport only applies `Empty`, `NoteOn`, and `NoteOff`, with a single fixed `frames_per_row`: `src/transport.cpp:8-42`.
- The synth voice is a single oscillator with waveform, phase, phase step, and noise state; there is no gain envelope, pitch modulation state, filter, or effect bus: `src/synth.hpp:11-31`, `src/synth.cpp:79-121`.
- Tempo and waveform are only set globally from CLI flags, not from pattern rows: `src/main.cpp:58-60`, `src/main.cpp:119-133`, `src/main.cpp:323-331`.
- The renderer duplicates one mono voice to all output channels, which means there is no mixer graph yet: `src/audio.cpp:34-40`.
- The terminal view highlights a whole source line; it is not a cell-based tracker UI with note/instrument/FX columns: `src/terminal_display.cpp:93-133`.

## Current Capability Snapshot

Today the app is effectively this:

- One monophonic track
- One event per row
- Events are only `NoteOn`, `NoteOff`, or `Empty`
- One simple oscillator voice
- Global startup `BPM`, `LPB`, and waveform from CLI
- Live reload at loop boundaries

That means every M8 command first needs at least a row-FX syntax and compiled FX model before it can even be parsed.

## Commands That Fit The Current App Scope

These commands are realistic additions while keeping `gaga` single-track, monophonic, and oscillator-based.

| Command | Fit | Why it fits | Missing functionality |
| --- | --- | --- | --- |
| `VOL XX` | Good candidate | A voice gain offset maps cleanly onto the current oscillator. | FX row syntax, compiled FX storage, gain state in synth/render path. |
| `PIT XX` | Good candidate | Semitone pitch offsets fit the existing note-frequency model. | FX row syntax, pitch offset state, note-range handling. |
| `FIN XX` | Good candidate | Fine pitch offset is a smaller version of `PIT`. | FX row syntax, cents/fine-tune state in voice or transport. |
| `TPO XX` | Good candidate | Tempo already exists globally via `--bpm`; row-level tempo changes are a direct extension. | Row-level tempo events, transport updates to `frames_per_row` at row boundaries. |
| `TSP XX` | Good candidate | Global transpose is straightforward in a one-track engine. | Global transpose state and note clamp/wrap rules. |
| `CHA XY` | Good candidate | Probability gating can work on a single track. | Deterministic RNG state, command evaluation rules, per-row FX representation. |
| `NTH XY` | Good candidate | Loop-count conditions fit existing looped playback. | Loop-pass counter and conditional row execution. |
| `SED XX` | Good candidate | Track-local random seed is useful once random/conditional FX exist. | RNG state in track/transport and stable seed semantics. |
| `OFF XX` | Good candidate | The app already has immediate `OFF`; delayed note-off is the same idea with sub-row timing. | Tick/sub-row scheduler and release-vs-kill behavior rules. |
| `KIL XX` | Good candidate | Timed hard-stop is a small extension of note-off logic. | Tick/sub-row scheduler and distinct hard-stop semantics. |
| `DEL XX` | Good candidate | Delaying the current row stays inside one track and one voice. | Sub-row event scheduler and tick resolution. |
| `RET XY` | Good candidate | Retriggering a note inside a row is compatible with a monophonic voice. | Sub-row scheduler, retrigger count/spacing, optional gain ramping. |
| `MTT XX` | Good candidate | Micro-timing is transport-only; it does not need new musical subsystems. | Finer-than-row timing resolution and event offsets. |
| `PSL XX` | Good candidate | Portamento is natural for a single oscillator voice. | Target-frequency glide state and glide rate handling. |
| `PBN XX` | Good candidate | Continuous bend is a direct extension of voice pitch. | Per-sample pitch modulation state. |
| `PVB XY` | Good candidate | Vibrato fits a single-voice synth. | Vibrato state, speed/depth handling, phase continuity. |
| `PVX XY` | Good candidate | Same as `PVB`, just with a wider range. | Same as `PVB`, with larger modulation depth. |
| `ARP XY` | Good candidate | A monophonic arpeggiator can cycle pitch within the one voice. | Sub-row scheduler, arp state, interval interpretation. |
| `ARC XY` | Good candidate | Arp mode/speed configuration is an extension of `ARP`. | Persistent arp settings and arp playback rules. |
| `VMV XX` | Optional candidate | Main volume can be implemented as a simple master gain without a full mixer. | Master gain stage plus row-FX syntax. |

## Commands That Need A Larger Sequencer Redesign

These still fit a "simple tracker" direction, but not with the current row model.

| Command | Why it is not a first-wave addition | Missing functionality |
| --- | --- | --- |
| `GRV XX` | Groove conflicts with the current fixed `frames_per_row` model and with the v1 spec's "no swing or groove" scope. | Variable row-duration or swing map in transport. |
| `RND XY` | It depends on modifying the previously active FX command, but the app has no generic FX history yet. | Multiple FX per row, FX execution history, deterministic mutation rules. |
| `RNL XY` | "Command to the left" is an M8 column concept that the current one-token row format cannot represent. | Multi-column row model, command-addressing rules, optionally note/instrument/velocity fields. |
| `REP XX` | Repeating and incrementing the last FX command implies a stateful FX interpreter. | FX history, per-row state machine, stop/reset rules. |
| `RTO XX` | It only makes sense once `REP` exists. | `REP` engine plus repetition bounds. |
| `SCA XY` | The app uses explicit chromatic note names, not scale-relative note semantics. | Track key/scale state and note-to-scale mapping rules. |
| `SCG XY` | Same issue as `SCA`, but at global scope. | Global key/scale state and note translation semantics. |

## Commands Blocked By Missing Major Subsystems

These commands are not good fits until the app gains subsystems it explicitly does not have today.

### Multi-track / phrase / song structure

| Command | Missing functionality |
| --- | --- |
| `GGR XX` | Multiple tracks plus a shared groove controller. |
| `HOP XY` / `HOPFF` | Phrase boundaries, next-phrase jumps, and track stop control at the phrase/song level. |
| `RMX XY` | Multiple track playheads that can be repositioned independently. |
| `SNG XX` | Song rows / pattern chain / arrangement mode. |
| `NXT XX` | At least two tracks plus cross-track triggering and velocity/instrument routing. |
| `VT1-VT8 XX` | Multiple independently mixed tracks. |

### Instruments and tables

| Command | Missing functionality |
| --- | --- |
| `INS XX` | Instrument definitions and instrument switching. |
| `TBL XX` | Instrument table assignment. |
| `THO XX` | Table playback state and table row jumps. |
| `TIC XX` | Table tick modes and table clocking. |
| `TBX XX` | Auxiliary parallel table playback. |
| `EQI XX` | Per-instrument EQ slots and an instrument mixer path. |

### Mixer / sends / DSP effects

| Command | Missing functionality |
| --- | --- |
| `EQM XX` | Main-mix EQ slots and EQ processing. |
| `XMT XX`, `XMM XX`, `XMF XX`, `XMW XX`, `XMR XX` | ModFX engine, stereo routing, and send paths. |
| `XDT XY`, `XDF XX`, `XDW XX`, `XDR XX` | Delay engine and delay-to-reverb routing. |
| `XRS XX`, `XRD XX`, `XRM XX`, `XRF XX`, `XRW XX`, `XRZ XX` | Reverb engine and freeze support. |
| `VMX XX`, `VDE XX`, `VRE XX` | Effect return buses. |
| `DJC XX`, `DJR XX`, `DJT XX` | DJ filter DSP and global insert routing. |

### Audio inputs / external routing

| Command | Missing functionality |
| --- | --- |
| `IVO XX`, `IMX XX`, `IDE XX`, `IRV XX` | Analog input capture and routing into internal FX buses. |
| `IV2 XX`, `IM2 XX`, `ID2 XX`, `IR2 XX` | Dual-mono second input path. |
| `USB XX` | USB audio input capture and gain control. |

### Richer instrument modulation

| Command | Missing functionality |
| --- | --- |
| `EA1`, `EA2` | Envelope generators and envelope amount parameters. |
| `AT1`, `AT2` | Envelope attack stage parameters. |
| `HO1`, `HO2` | Envelope hold stage parameters. |
| `DE1`, `DE2` | Envelope decay stage parameters. |
| `ET1`, `ET2` | Envelope retrigger behavior. |
| `LA1`, `LA2` | LFO amount parameters. |
| `LF1`, `LF2` | LFO frequency parameters. |
| `LT1`, `LT2` | LFO phase/start/retrigger control. |

## What Needs To Change First

To unlock even the "good candidate" commands, the app needs a larger row/FX model than it has today.

### 1. Pattern format and parser

Minimum parser/model work:

- recognize generic 3-letter FX mnemonics plus operands
- allow more than one token per row
- compile rows into note data plus zero or more FX commands
- stop treating every extra token as a parse error

Concrete code that would need to change first:

- `src/tokenizer.cpp:68-153`
- `src/parser.cpp:75-135`
- `src/pattern.hpp:10-30`
- `src/terminal_display.cpp:93-133`

### 2. Transport and timing

Minimum transport work:

- support row-boundary state changes such as tempo and transpose
- add a tick or sub-row scheduler for delayed / retriggered / microtimed events
- optionally support variable row durations for groove

Concrete code that would need to change first:

- `src/transport.cpp:8-42`
- `src/audio.cpp:53-126`

### 3. Voice state

Minimum synth work:

- add gain state
- add pitch offset / fine tune / transpose state
- add glide and vibrato state
- optionally add deterministic RNG state on the track side

Concrete code that would need to change first:

- `src/synth.hpp:19-31`
- `src/synth.cpp:79-121`

## Recommended Implementation Order

If the goal is to get the most useful M8-style commands into the app without turning it into a full workstation, this is the best order:

1. Add a row-FX syntax and compiled FX representation.
2. Add row-boundary state commands: `VOL`, `PIT`, `FIN`, `TPO`, `TSP`, `CHA`, `NTH`, `SED`, and optionally `VMV`.
3. Add a sub-row scheduler: `OFF XX`, `KIL`, `DEL`, `RET`, `MTT`.
4. Add pitch-motion commands: `PSL`, `PBN`, `PVB`, `PVX`, `ARP`, `ARC`.
5. Re-evaluate whether the project should stay "simple tracker plus FX" or grow into instruments, tables, multi-track sequencing, and a mixer.

That first four-step roadmap would cover the highest-value M8-style commands that still match the current app's single-track, monophonic design.
