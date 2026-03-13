# gaga

## Build

From a Visual Studio developer shell:

```powershell
cmake -S . -B build-msvc -G Ninja
cmake --build build-msvc
```

Or use the helper script in this repo:

```powershell
./build_local.bat
./build_local.bat release
```

## Use

Run the tracker with a `.gaga` pattern file:

```powershell
build-msvc\gaga.exe path\to\pattern.gaga
```

Or build and run in one step:

```powershell
./run_local.bat path\to\pattern.gaga
./run_local.bat release path\to\pattern.gaga --trace
```

Optional flags:

```powershell
build-msvc\gaga.exe path\to\pattern.gaga --loop
build-msvc\gaga.exe path\to\pattern.gaga --bpm 120 --lpb 4
build-msvc\gaga.exe path\to\pattern.gaga --synth square
build-msvc\gaga.exe path\to\pattern.gaga --trace
build-msvc\gaga.exe path\to\pattern.gaga --analyze-audio
build-msvc\gaga.exe path\to\pattern.gaga --render-wav out.wav
```

While playback is running in the interactive terminal view, press `Esc` to stop and exit.

## Audio Debugging

Use the offline debug path when you need deterministic evidence about what the
synth/transport generated:

```powershell
build-msvc\gaga.exe path\to\pattern.gaga --analyze-audio
build-msvc\gaga.exe path\to\pattern.gaga --render-wav build-debug\pattern.wav
```

`--analyze-audio` prints one-pass metrics such as peak, RMS, DC offset,
silence, clipping, non-finite samples, and discontinuity counts, then exits
without opening the audio device. `--render-wav` writes the same one-pass
render to disk and also exits immediately, which is useful for listening tests
and future regression comparisons.

Supported synth waveforms for `--synth` are `sine`, `square`, `saw`,
`triangle`, and `noise`.

## Pattern Syntax

Each row starts with one of:

- a note like `C4`, `F#3`, or `C-1`
- `OFF`
- `---`

Rows can also carry optional M8-style `volume` and `instrument` columns before any FX pairs:

```text
C4 64 saw
D4 triangle
--- noise
```

Column rules:

- `volume`: `00` to `7F`, or `--` when omitted
- `instrument`: one of `sine`, `square`, `saw`, `triangle`, `noise`
- a note row without an `instrument` column keeps the current instrument and changes pitch without retriggering it

Rows can also carry zero or more FX pairs after the row event or optional columns:

```text
C4 64 square VOL 20
D4 triangle
--- PIT 01 FIN 40
--- noise TSP FF TPO 90 VMV 80
OFF VOL E0
```

Supported row FX today:

- `VOL XX`: signed volume offset where `00` is neutral, positive values get louder, and values like `E0` reduce gain
- `PIT XX`: signed semitone offset where `01` is +1 semitone and `FF` is -1 semitone
- `FIN XX`: signed fine pitch offset where the byte maps to about +/-1 semitone across the full range
- `TSP XX`: signed global transpose in semitones, applied to the current and following notes
- `TPO XX`: absolute tempo in BPM, encoded as a hex byte from `01` to `FF`
- `VMV XX`: absolute master volume from `00` (mute) to `FF` (full scale)

The `instrument` column is a names-only selector for the built-in waveforms. This is not a full Dirtywave M8 instrument implementation. It is a small built-in layer over the existing single-voice synth so the column is immediately usable in `gaga`.

Scientific pitch notation is canonical. Notes use the MIDI-aligned `C-1` to `G9`
range, where `C4` is middle C. Pattern tokens are case-insensitive.
Normalized output prints uppercase note names, column values, FX names, and hex values.
