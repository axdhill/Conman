# Conman

A JUCE audio plugin with two convolution modes:

**Real-time mode** — Load an impulse response and convolve live audio input with it in real time (convolution reverb, cabinet simulation, etc.). Supports drag-and-drop IR loading, dry/wet mix, and output gain controls.

**Offline mode** — Load two audio samples, convolve one with the other, and export the result as a 24-bit WAV file. Runs on a background thread so the UI stays responsive.

## Building

Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.4 is fetched automatically.

**macOS (with Xcode):**

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

**macOS / Linux (Makefiles):**

```bash
cmake -B build
cmake --build build --config Release
```

Build outputs are in `build/Conman_artefacts/Release/`:
- `AUv3/Conman.appex` — AUv3 plugin (macOS, requires Xcode)
- `Standalone/Conman.app` — Standalone application

## Installing (macOS)

```bash
cp -r build/Conman_artefacts/Release/AUv3/Conman.appex \
  ~/Library/Audio/Plug-Ins/Components/
```

Rescan plugins in your DAW if needed.

## Usage

### Real-time convolution
1. Load an impulse response WAV/AIFF/FLAC using the **Load IR** button or by dragging a file onto the plugin window
2. Adjust **Dry/Wet** to blend between the original and convolved signal
3. Adjust **Gain** to set the output level

### Offline convolution
1. Load two audio files using **Load Sample A** and **Load Sample B**
2. Click **Convolve & Export** and choose an output location
3. The result is written as a 24-bit WAV, normalized to prevent clipping

## License

[MIT](LICENSE)
