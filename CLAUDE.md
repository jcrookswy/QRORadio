# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

This is a Visual Studio 2019/2022 project (MSVC v142 toolchain). Build via the IDE or MSBuild:

```powershell
# Debug x64 (primary config)
msbuild wx1.sln /p:Configuration=Debug /p:Platform=x64

# Release x64
msbuild wx1.sln /p:Configuration=Release /p:Platform=x64
```

There are no tests, no lint steps, and no CMake. The x86 configurations target `D:\wxWidgets-3.2.6\` and are not the active development path; use x64.

## Dependencies

All must be pre-installed; nothing is downloaded at build time.

| Dependency | Location (x64) | Purpose |
|---|---|---|
| wxWidgets 3.2.6 | `C:\Users\jcroo\wxWidgets-3.2.6\` | GUI framework |
| PortAudio | Bundled in repo root (`portaudio_x64.dll/.lib`, `portaudio.h`) | Audio I/O |
| Intel oneAPI IPP | `C:\Program Files (x86)\Intel\oneAPI\ipp\latest\` | DSP primitives (FFT, DFT, resampler) |
| Win32 serial API | OS | Serial communication with radio hardware |

## Architecture

This is a wxWidgets GUI application for a custom HF SDR transceiver (the QRO20 hardware). There are no unit tests; `Debug\` output is the working directory for the executable.

### Files that matter

- **`CRadio.cpp` / `CRadio.h`** — The entire radio backend: serial connection (`Connect()`), RX DSP loop (`RXDataLoop()`), TX DSP loop (`TXDataLoop()`), VNA/antenna analyzer loop (`AntTuneDataLoop()`), settings I/O, and all IPP-based signal processing.
- **`wx1.cpp`** — wxWidgets entry point (`MyApp`), all `MyFrame` button/event handlers, and all custom drawing functions (`Plot()`, `SmithPlot()`, `BasicDrawPane::render()`).
- **`frame1.h`** — `MyFrame` and `BasicDrawPane` class declarations. Despite the wxFormBuilder header comment, this file **is** manually edited.
- **`DSP.cpp`** — Contains only the polyphase resampler coefficient table (512-element sinc kernel). Not compiled into the project currently (not in `wx1.vcxproj`).

### Files to ignore

- **`CRadioBroken.cpp`** and **`CRadiotemp.cpp`** — Old backup snapshots of `CRadio.cpp`. Not compiled (absent from `wx1.vcxproj`). Do not edit.
- **`txtest/`** — Standalone TX DSP testbed using WAV I/O. Separate from the main project.

### Mode state machine

`RadioStatus::mode` (in `CRadio.h`) drives everything:

```
IDLE_MODE (0) → RX_MODE (1) → TX_MODE (2) → VNA_MODE (3)
```

`CRadio::DataThread()` loops on `myStatus->mode` and dispatches to `RXDataLoop()`, `TXDataLoop()`, or `AntTuneDataLoop()`. Mode changes are initiated from the UI thread (button clicks in `wx1.cpp`) and detected by the data thread polling `myStatus->mode`.

### Serial protocol

The hardware uses 8N1 serial at a configurable COM port. Commands are single lowercase ASCII bytes:
- `'a'` — query ADC (voltage/current), returns 4 bytes
- `'b'` — request one block of 250 I/Q samples, returns 2000 bytes
- `'f'` followed by frequency bytes — set LO frequency
- `'v'` — enter VNA mode

IQ data encoding: 6-bit ASCII offset (each byte `- 0x20`), 4 bytes per I or Q value (6 bits each), yielding 24-bit signed integers. Swap: hardware sends Q first, then I.

### RX DSP path

`RXDataLoop()` feeds `ProcessIQ()` → `DoRXDSP()`:
1. Accumulate IQ samples into `RawIQData[]` ring buffer (16000 samples)
2. Every 250 samples: apply Hann window, 250-point DFT, mask USB bins 1–15, 256-point IFFT
3. Overlap-add accumulation → `RawAudio[]` → `audioOutBuf[]` ring for PortAudio playback
4. ALC (automatic level control) on `TunerMag`
5. Spectrum: `MagData[]` min/max accumulation for waterfall/spectrum plot

### TX DSP path (CESSB)

`TXDataLoop()` reads microphone audio from `audioInBuf[]` and produces IQ to send to hardware:
1. 300 Hz HPF + 3 kHz LPF biquad filters
2. CESSB (Controlled Envelope SSB) via overlap-save 2048-pt FFT: 3 iterations of clip→filter
3. Polyphase resampler (48 kHz → hardware rate) for both I and Q channels
4. `ENABLE_CESSB` macro in `CRadio.cpp` can be commented out to bypass envelope clipping

### VNA/antenna analyzer

`AntTuneDataLoop()` sweeps 36 frequencies (10 kHz steps from 14.0–14.35 MHz). OSL calibration (`AntTuneSweepOSL()`) stores Open/Short/Load complex S11 measurements in `AntTuneCal`. `GetCorrectedS11()` applies the 3-term error correction.

### UI / rendering

`OnTimer()` (100 ms wxTimer) calls `BasicDrawPane::Refresh()` when `RadioStatus` update flags are set. `BasicDrawPane::render()` in `wx1.cpp` draws all panels directly with wxDC — no retained scene graph. `wfPixels[]` is the 100×250 RGB waterfall pixel buffer updated each render.

### Settings

`CRadio::SaveSettings()` / `LoadSettings()` write a hand-rolled JSON file (`settings.json`) containing COM port, LO frequency, relay settings, callsign, and the four 36-element VNA calibration arrays. Loaded automatically on `Connect()`.
