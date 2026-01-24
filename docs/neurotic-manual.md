# Neurotic Manual

Neurotic is a 10‑algorithm, neural‑inspired processor for the kxmx_bluemchen hardware. Each algorithm has its own page and name at the top line. The **title line** is the algorithm selector (rotate encoder to change). Below the title are shared controls plus two algorithm‑specific parameters.

## Hardware Controls
- **Knob 1 + CV1 (C1)**: Primary performance control (algorithm‑specific).
- **Knob 2 + CV2 (C2)**: Secondary performance control (algorithm‑specific).
- **Encoder**: 
  - Rotate on **title line** → change algorithm.
  - Press to move selection down the list.
  - Rotate on an item → change its value.

## Menu Layout (single page)
Top line shows the algorithm name. Items underneath (same on every algorithm):
1. **MIX** – Dry/wet balance (default 100% wet).
2. **LDEPTH** – LFO depth (applies where relevant).
3. **LRATE** – LFO rate.
4. **OUT** – Output trim.
5. **Param 1 (C3)** – Algorithm‑specific.
6. **Param 2 (C4)** – Algorithm‑specific.

## Algorithms

### 0. CrossRes (Neural Cross‑Resonator)
A resonant “virtual body” with modal density and stereo asymmetry.
- **C1 Mass**: changes modal density / low‑end weight.
- **C2 Tension**: shifts resonant frequencies.
- **C3 MASS**: density/weight emphasis.
- **C4 ASYM**: stereo dispersion.

### 1. Braid (Latent Spectral Braider)
STFT‑based cross‑spectral braid between inputs. Stronger settings exchange identity.
- **C1 Braid Depth**: amount of partial exchange.
- **C2 Formant Swap**: spectral envelope mapping.
- **C3 FORM**: envelope bias.
- **C4 TRANS**: transient protection.

### 2. TapeHyd (Neural Tape Hydraulics)
Tape‑like delay with flutter and saturation.
- **C1 Drive**: saturation intensity.
- **C2 Flow**: modulation depth and speed.
- **C3 HEAD**: high‑frequency loss / smear width.
- **C4 FDBK**: feedback amount.

### 3. Binaural (Binaural Gesture Mapper)
Spatial movement with ITD/ILD effects and distance filtering.
- **C1 Azimuth**: left‑right orbit.
- **C2 Elevation**: top/bottom placement.
- **C3 Distance**: near/far filtering.
- **C4 SPIN**: rotating motion.

### 4. Formant (Neural Formant Forge)
Formant lattice for vocal/reed tones.
- **C1 Vowel Pull**: locks toward vowel‑like resonances.
- **C2 Articulation**: smooth vs. percussive movement.
- **C3 BREATH**: noise/air injection.
- **C4 SPLIT**: stereo formant divergence.

### 5. Diffusion (Neural Diffusion Multiband)
Smears energy in time and frequency with drift.
- **C1 Spread**: diffusion extent.
- **C2 Color**: spectral tilt.
- **C3 Grain**: diffusion granularity.
- **C4 Drift**: stereo drift.

### 6. Energy (Neural Energy Shaper)
Dynamics reshaper with punch/glue/lift.
- **C1 Punch**: transient emphasis.
- **C2 Glue**: linking between channels.
- **C3 Lift**: upward energy tilt.
- **C4 Bias**: which channel dominates.

### 7. Harmonic (Neural Harmonic Cartographer)
Remaps partials into stretched/inharmonic spectra.
- **C1 Stretch**: harmonic spacing.
- **C2 Inharm**: bends partials off‑grid.
- **C3 Sparse**: selects fewer partials.
- **C4 Mirror**: folds highs into lows.

### 8. PhaseLoom (Neural Phase Loom)
Phase re‑weaving with swirl and tilt.
- **C1 Bind**: lock phases between channels.
- **C2 Swirl**: nonlinear phase warping.
- **C3 Tilt**: low/high phase skew.
- **C4 Stereo**: widen/narrow.

### 9. MicroGran (Neural Micro‑Granulator)
Granular micro‑texture from the input.
- **C1 Size**: grain size.
- **C2 Drift**: pitch/time variance.
- **C3 Blend**: dry vs granulated.
- **C4 Scatter**: stereo grain spread.

## Notes
- LFO affects algorithms where modulation makes sense (e.g., Braid, TapeHyd, Diffusion, PhaseLoom, Binaural).
- MIX is full wet by default; set to taste for parallel processing.
