# Slime User Manual

Spectral processor firmware for kxmx_bluemchen.

## Overview

Slime splits audio into FFT frames, applies a spectral effect, then reconstructs with overlap‑add. Both channels run independently, with channel 2 optionally time‑scaled relative to channel 1.

## Panel Controls

- **Knob 1 + CV1**: Time (spectral smear rate) for channel 1. Bipolar around knob center.
- **Knob 2 + CV2**: Vibe amount (process‑specific intensity). Bipolar around knob center.
- **Encoder rotate**: Adjust the active menu item.
- **Encoder press**: Cycle menu pages.

## Menu Pages

1. **Process**: Select spectral algorithm.
2. **Time Ratio**: Multiplier for channel 2 relative to channel 1 (0.125x–8x).
3. **Mix**: Dry/wet balance (0–100%).
4. **Debug**: Shows page/process indices and CV readouts.
5. **ADC Raw**: Shows raw ADC values for Knob 1/2 and CV 1/2.

## Processes

- **SMR (Smear)**: Blurs energy across neighboring bins.
- **SFT (Shift)**: Shifts spectral content up/down.
- **CMB (Comb)**: Periodic bin emphasis for formant‑like color.
- **FRZ (Freeze)**: Holds magnitudes for pad‑like sustain.
- **GAT (Gate)**: Spectral gate by magnitude.
- **TLT (Tilt)**: Spectral tilt EQ.
- **FLD (Fold)**: Mirror bins around a center.
- **PHS (Phase)**: Phase warp for motion.

## Display

The OLED shows:

- **Top row**: `Slm <PROC>` + heartbeat dot.
- **Row 2**: `T1` time ratio for channel 1 (x100).
- **Row 3**: `T2` time ratio for channel 2 (x100).
- **Row 4**: Menu value (Vibe, Ratio, or Mix).
- **Debug page**: `Pg/Pn` plus `C1/C2` (normalized CV).
- **ADC Raw page**: `K1/K2/C1/C2` hex raw values.

## Tips

- Feed short percussive sounds for dramatic time smear.
- Use small Vibe values for subtle texture, higher for dramatic shifts.
- Set Time Ratio for stereo motion (e.g., 1.0 vs 1.5).
