# Slime Spectral Processor

A two-channel spectral effect: audio is transformed with FFT, modified in the frequency domain, then reconstructed with IFFT and overlap-add.

## DSP Overview

- **Pipeline**: Input -> window -> FFT -> spectral process -> IFFT -> window -> overlap-add -> dry/wet mix.
- **Channels**: Two independent spectral streams (L/R), with optional time ratio linking.
- **FFT size**: 1024 samples (power-of-two), hop size 256 (4x overlap) to keep transient smear reasonable while enabling time stretch.
- **Window**: Hann window for both analysis and synthesis.
- **Latency**: ~FFT size + hop overlap (expect ~20–25 ms at 48 kHz).

## Time Stretch ("Time")

- Time stretch is implemented as frame-to-frame magnitude smoothing (time smear) in the spectral domain.
- **Range**: 0.25x to 16x (higher values = slower spectral evolution).
- **Mapping**: Pot 1 + CV1 set a base time ratio for channel 1; channel 2 uses base * ratio (see menu).
- **Stability**: Clamp extreme values to avoid NaN/inf and keep CPU bounded.

## Vibe ("Spectral Spread")

- Vibe controls spectral spread/warp; the exact behavior depends on the selected process.
- **Global**: Pot 2 + CV2 set the Vibe amount.
- **Typical behaviors**:
  - **Bin Smear**: Spread energy across neighboring bins (spectral blur).
  - **Harmonic Shift**: Scale bin indices to "tilt" the spectrum (melodic pitch drift).
  - **Comb Mask**: Apply periodic bin emphasis for vowel-like formants.
  - **Freeze**: Hold magnitude envelope while phase continues (pad-like textures).

## Process Menu

The process menu selects which frequency-domain algorithm runs per channel:

1. **Smear**: Neighbor-bin averaging with Vibe controlling spread radius.
2. **Shift**: Bin index scaling; Vibe controls shift amount.
3. **Comb**: Periodic bin mask; Vibe controls comb spacing.
4. **Freeze**: Magnitude hold; Vibe controls decay time.

Each process should be implemented as a single switch in the spectral loop to keep CPU bounded.

## Controls and Menu

- **Knob 1 + CV1**: Time (base) for channel 1.
- **Knob 2 + CV2**: Vibe amount.
- **Encoder rotation**: Edits current menu item value.
- **Encoder press**: Cycles menu pages.

Menu pages:

1. **Process**: Select spectral algorithm.
2. **Time Ratio**: Multiplier for channel 2 relative to channel 1 (0.25x–4x).
3. **Mix**: Dry/wet mix (0–100%).

## Display

OLED should show:

- Top row: `Slime <PROC>` plus heartbeat dot.
- Row 2: `T1` and current time ratio for channel 1.
- Row 3: `T2` and derived time ratio for channel 2.
- Row 4: Vibe amount or current menu value (depends on active page).

## Implementation Notes

- Use fixed buffers for FFT and overlap-add; no dynamic allocation in the audio callback.
- Process controls in the main loop; audio callback reads cached parameters.
- Clamp Vibe and Time parameters to safe ranges to prevent NaN/inf in spectral math.
- Make spectral processes deterministic frame-to-frame to avoid jitter.

