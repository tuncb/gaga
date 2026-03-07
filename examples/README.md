## Examples

These files are valid `.gaga` patterns you can use as starting points.

Run one with:

```powershell
build-msvc\gaga.exe examples\hello_scale.gaga
build-msvc\gaga.exe examples\twinkle_excerpt.gaga --loop
build-msvc\gaga.exe examples\bass_walk.gaga --bpm 100 --lpb 4
```

What to edit:

- Change any note token like `C-4` or `F#3`.
- Use `---` to keep the current note playing.
- Use `OFF` to stop the current note.
- Add `# comments` anywhere on a line after a valid token.
