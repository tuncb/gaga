## Examples

These files are valid `.gaga` patterns you can use as starting points.

Run one with:

```powershell
build-msvc\gaga.exe examples\hello_scale.gaga
build-msvc\gaga.exe examples\twinkle_excerpt.gaga --loop
build-msvc\gaga.exe examples\bass_walk.gaga --bpm 100 --lpb 4
build-msvc\gaga.exe examples\fx_offsets.gaga --trace
build-msvc\gaga.exe examples\fx_volume_swell.gaga --trace
build-msvc\gaga.exe examples\fx_pitch_steps.gaga --trace
build-msvc\gaga.exe examples\fx_fine_chime.gaga --trace
build-msvc\gaga.exe examples\fx_transpose_hook.gaga --trace
build-msvc\gaga.exe examples\fx_tempo_switch.gaga --trace
build-msvc\gaga.exe examples\fx_master_pulse.gaga --trace
build-msvc\gaga.exe examples\m8_columns.gaga --trace
```

What to edit:

- Change any note token like `C4`, `C-1`, or `F#3`.
- Use `---` to keep the current note playing.
- Use `OFF` to stop the current note.
- Add row FX like `VOL 20`, `PIT 01`, or `FIN F0` after the row token.
- Add row FX like `TSP FF`, `TPO 90`, or `VMV C0` after the row token.
- Add `# comments` anywhere on a line after a valid token.

FX-focused examples:

- `fx_offsets.gaga`: minimal one-file overview of the row FX set.
- `fx_volume_swell.gaga`: uses `VOL` for accents and fade-like swells.
- `fx_pitch_steps.gaga`: uses `PIT` to move sustained notes in semitone steps.
- `fx_fine_chime.gaga`: uses `FIN` for subtle detune motion.
- `fx_transpose_hook.gaga`: uses `TSP` to move one riff across keys.
- `fx_tempo_switch.gaga`: uses `TPO` for in-pattern tempo changes.
- `fx_master_pulse.gaga`: uses `VMV` to shape the whole output level.
- `m8_columns.gaga`: demonstrates M8-style volume and instrument columns with legato note changes.
