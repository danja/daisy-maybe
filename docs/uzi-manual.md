# Uzi Spectral Phaser Manual

Uzi is a stereo spectral phaser built on the Slime FFT core, with a distortion front-end and menu-driven UI.

## Signal Path

1. Input
2. Wavefolding + soft/hard clipping (distortion)
3. FFT / spectral notches / crossover / blur
4. IFFT
5. Dry/wet mix

Low-end content under ~250 Hz is preserved.

## Front-Panel Controls

- **Knob 1 + CV1**: Spectral notch distance (log-mapped).
- **Knob 2 + CV2**: Spectral phase offset.
- **Encoder rotate**: Adjust current item or switch pages when the title is selected.
- **Encoder press**: Cycle selection (title → items → title).

## Menu Pages

### Master
- **MIX**: Dry/wet balance.
- **FBK**: Feedback amount (spectral loopback).
- **X**: Cross-pollinate spectral magnitude/phase between channels.
- **CUT**: Low-end passthrough cutoff (0-300 Hz).
- **LFD**: LFO depth (modulates spectral phase offset).
- **LFR**: LFO frequency.

### Distortion
- **WAVE**: Wavefold depth.
- **ODRV**: Overdrive amount (soft→hard blend).

### FFT
- **XOVR**: Spectral crossover between channels.
- **BLUR**: Notch sharpness (higher = wider/blurred).
- **BINS**: Spectral bin rounding (quantization of notch positions).
- **BLK**: Block size / overlap mode.
  - 0: Hop 128
  - 1: Hop 256
  - 2: Hop 512

## Notes

- Block size changes overlap behavior; higher values trade smoothness for latency/CPU.
- LFO depth at 0 disables modulation.
