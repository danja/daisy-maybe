# Uzi Spectral Phaser - Implementation Plan

## Overview
Uzi is a new firmware subproject built on the same DSP and UI foundations as `slime`. It is a two-channel spectral phaser with a distortion front-end, FFT-domain spectral notching, and a menu-driven UI. The plan below follows the repository guidance and modularity requirements.

## Milestones

### 1) Project Skeleton + Build
- Create a new `uzi/` folder at repo root.
- Add `Makefile` mirroring `slime/Makefile`, including `$(LIBDAISY_DIR)/core/Makefile` and referencing `kxmx_bluemchen/src/kxmx_bluemchen.cpp`.
- Add `main.cpp` and minimal support files (display, encoder, DSP helpers).
- Verify `make -C uzi` builds.

### 2) Core App Architecture (Modular)
- Define module boundaries:
  - `UziApp` (lifecycle, audio callback, main loop)
  - `UziUi` (menu state, parameter editing, screen rendering)
  - `UziDsp` (signal chain: distortion -> FFT -> spectral processing -> IFFT)
  - `UziParams` (parameter definitions, smoothing, CV/knob mapping)
  - `UziState` (shared runtime state, presets if needed)
- Keep DSP allocation-free inside the audio callback.
- Run control scanning and menu updates in the main loop at ~30 Hz.

### 3) DSP Signal Chain
- Two-channel processing path.
- Pre-FFT distortion section:
  - Wavefolding stage (match resonators behavior).
  - Soft/hard clipping stage (continuous blend).
- FFT/IFFT:
  - Reuse slime FFT implementation.
  - Use Hann window.
- Spectral processing:
  - Preserve frequencies under 250 Hz (bypass bins below cutoff).
  - Spectral notches controlled by distance and phase offset.
  - Blur controls notch sharpness.
  - Spectral bin rounding control.
  - Block size parameter with safe constraints and restart handling.
- Cross-channel spectral mix:
  - Frequency-domain crossover between channel 1 and 2.

### 4) Controls + Modulation
- CV1 + Knob1: notch distance (log mapping).
- CV2 + Knob2: spectral phase offset.
- Add smoothing for all parameters.
- Add LFO modulation for depth/frequency affecting spectral phase or notch position as appropriate.

### 5) UI/Menu System
- Follow `docs/menu-system.md`.
- Menu pages:
  - Master: Mix, LFO Depth, LFO Frequency.
  - Distortion: Wave, Overdrive.
  - FFT: Crossover, Blur, Bin Rounding, Block Size.
- Encoder navigation and press actions consistent with other firmware.
- OLED refresh ~30 Hz.

### 6) Validation and Tuning
- Verify dry/wet mix and phase behavior.
- Check low-end preservation (<= 250 Hz).
- Confirm stability across block sizes and parameter changes.
- Test CPU usage and optimize FFT size transitions.

### 7) Docs and Integration
- Add `docs/uzi-manual.md` with UI + parameter descriptions.
- Update `README.md` to list Uzi.

## File Breakdown (proposed)
- `uzi/main.cpp`
- `uzi/uzi_app.h/.cpp`
- `uzi/uzi_dsp.h/.cpp`
- `uzi/uzi_ui.h/.cpp`
- `uzi/uzi_params.h/.cpp`
- `uzi/uzi_state.h/.cpp`

## Notes and Constraints
- Avoid allocations in audio callback.
- Keep comments only where operations are non-obvious.
- Use existing DSP helpers from `slime` and distortion from `resonators` as references.
