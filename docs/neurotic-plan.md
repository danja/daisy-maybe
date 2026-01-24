# Neurotic - Implementation Plan

## Overview
Neurotic is a new firmware project for the kxmx_bluemchen hardware, inspired by the 10 neural algorithm concepts in `docs/neural-ideas.md`. The implementation focuses on a modular DSP architecture that can host multiple “algorithms” (initially DSP approximations/stubs that map to the neural concepts), a unified menu/UI system per `docs/menu-system.md`, and a clean separation between hardware I/O, parameters, DSP, and display.

## Goals
- New firmware subproject `neurotic/` with modular DSP pipeline.
- 10 selectable algorithms with consistent control mapping (C1–C4 + menu items).
- Menu system compatible with the existing encoder behavior.
- Control inputs mapped to CV/knobs for performance, secondary parameters via menu.
- Keep audio callback allocation-free; control scanning in main loop.

## Control Mapping Strategy
- **Knob1 + CV1 → C1**, **Knob2 + CV2 → C2** (performance-critical).
- **Menu items** for C3/C4 and global parameters (mix, output trim, algorithm select, etc.).
- Use bipolar mapping for parameters that conceptually center around 0 (e.g., Phase/Stereo controls).

## Milestones

### 1) Project Skeleton
- Create `neurotic/` with Makefile, `main.cpp`, display/menu/encoder handlers.
- Reuse existing menu system from `resonators/` (menu_system.*) and display patterns.
- Add basic app loop + audio callback structure.

### 2) Modular DSP Architecture
- Define:
  - `NeuroticApp` (lifecycle, audio callback, main loop)
  - `NeuroticParams` (control input processing + smoothing)
  - `NeuroticUi` (menu pages, display rendering)
  - `NeuroticDsp` (algorithm dispatch)
  - `NeuroticAlgo` interface with 10 implementations
- Ensure no allocations in audio callback.

### 3) Algorithms (DSP Proxies)
Create 10 algorithm modules that mirror the neural concepts:
1. NCR: resonator/convolver-style networked body (dual comb + modal bank stub).
2. LSB: STFT-based cross-spectral braid (reuse slime FFT code in lighter mode).
3. NTH: tape-like delay + saturation + flutter.
4. BGM: binaural spatializer (simple HRTF-ish pan/phase model).
5. NFF: formant filter bank morph.
6. NDM: multi-band diffusion (chorus + smearing).
7. NES: dynamics shaper (dual compressor/expander coupling).
8. NHC: harmonic remapper (spectral shift + inharmonic stretch).
9. NPL: phase warp (phase rotation in STFT domain).
10. NMG: micro-granular texture (lightweight granular stub).

Each algorithm uses C1–C4 mapped to the concept parameters defined in `docs/neural-ideas.md`.

### 4) UI / Menu System
- Menu pages:
  - **Master**: Mix, Algorithm Select, Output Trim.
  - **Algo**: C3, C4 parameters (label varies by algorithm).
  - **Debug** (optional hidden page): raw CV/knob values.
- Encoder: rotate to adjust selected item; press to move selection / page.

### 5) Documentation & Integration
- Add `docs/neurotic-manual.md` describing algorithms + controls.
- Update `README.md` to include Neurotic.

## File Layout (proposed)
- `neurotic/main.cpp`
- `neurotic/neurotic_app.h/.cpp`
- `neurotic/neurotic_params.h/.cpp`
- `neurotic/neurotic_ui.h/.cpp`
- `neurotic/neurotic_dsp.h/.cpp`
- `neurotic/algos/` (10 algorithms)
- `neurotic/menu_system.*`, `neurotic/display.*`, `neurotic/encoder_handler.*`

## Notes
- Reuse FFT code from `slime/` if spectral algorithms need it.
- Keep UI updates ~30 Hz.
- Ensure stable audio even when algorithms are swapped.
