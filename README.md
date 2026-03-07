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
