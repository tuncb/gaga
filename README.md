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
```
