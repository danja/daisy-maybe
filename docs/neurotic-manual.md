# Neurotic Manual

Neurotic is an 11‑algorithm, neural‑inspired processor for the kxmx_bluemchen hardware. Each algorithm has its own page and name at the top line. The **title line** is the algorithm selector (rotate encoder to change). Below the title are shared controls plus two algorithm‑specific parameters.

## Hardware Controls
- **Knob 1 + CV1 (C1)**: Primary performance control (algorithm‑specific).
- **Knob 2 + CV2 (C2)**: Secondary performance control (algorithm‑specific).
- **Encoder**: 
  - Rotate on **title line** → change algorithm.
  - Press to move selection down the list.
  - Rotate on an item → change its value.

## Menu Layout (single page)
Top line shows the algorithm name. Items underneath (same on every algorithm):
1. **Mix** – Dry/wet balance (default 80% wet).
2. **Feed** – Global feedback amount (percent).
3. **Mod** – LFO depth (applies where relevant).
4. **Rate** – LFO rate (0.1–9.9 Hz).
5. **Param 1 (C3)** – Algorithm‑specific.
6. **Param 2 (C4)** – Algorithm‑specific.

## Algorithms

### 0. CrossRes (Neural Cross‑Resonator)
Two cascaded SVF band‑pass resonators per channel, followed by a one‑pole damping filter. The resonant center follows tension; stereo offset introduces detune between channels.
- **C1 Mass**: increases resonator spacing (modal density/spread).
- **C2 Tension**: base resonant frequency (exponential map).
- **C3 MASS**: damping amount (darker/shorter as it rises).
- **C4 ASYM**: stereo detune between L/R resonators.

### 1. Braid (Latent Spectral Braider)
STFT cross‑spectral braid: magnitudes and phases of L/R are interwoven per‑bin with transient protection. FFT is 1024 with 256‑sample hop and windowed overlap/add.
- **C1 Braid Depth**: strength of L/R magnitude exchange.
- **C2 Formant Swap**: bias of envelope sharing between channels.
- **C3 FORM**: transient protection (higher keeps attacks intact).
- **C4 TRANS**: additional weave emphasis across bands.

### 2. TapeHyd (Neural Tape Hydraulics)
Stereo tape‑style delay with modulation, soft saturation, and feedback tone shaping. Delay time is modulated by an internal LFO; feedback is low‑passed by head‑gap control.
- **C1 Drive**: saturation before the delays.
- **C2 Flow**: modulation depth and speed.
- **C3 HEAD**: head‑gap low‑pass (higher = darker).
- **C4 FDBK**: feedback amount.

### 3. Binaural (Binaural Gesture Mapper)
Stereo spatializer with interaural time differences (ITD), equal‑power panning, and distance filtering. A spin LFO adds orbiting movement.
- **C1 Azimuth**: left/right pan center.
- **C2 Elevation**: dry vs filtered distance blend.
- **C3 Distance**: mono low‑pass distance (near ↔ far).
- **C4 SPIN**: rotation amount (adds moving pan).

### 4. Formant (Neural Formant Forge)
Three SVF band‑pass formant filters per channel with adjustable spread. ARTIC injects noise into the excitation. BREATH adds “air” (high‑passed input) and stereo formant divergence.
- **C1 Vowel Pull**: base formant frequency.
- **C2 Articulation**: formant spacing/spread.
- **C3 ARTIC**: noise injection amount (default 0).
- **C4 BREATH**: air mix and stereo divergence.

### 5. Diffusion (Neural Diffusion Multiband)
Dual short delays with sinusoidal modulation and feedback smear. Color tilts dry contribution for a spectral skew.
- **C1 Spread**: delay depth/spread.
- **C2 Color**: spectral tilt between dry and delayed.
- **C3 Grain**: feedback amount in the diffusers.
- **C4 Drift**: modulation rate/depth.

### 6. Energy (Neural Energy Shaper)
Envelope‑linked dynamics shaper. Punch changes attack/release; glue links L/R envelopes; lift adds upward tilt; bias cross‑mixes channels.
- **C1 Punch**: transient emphasis (faster attack).
- **C2 Glue**: link between channels.
- **C3 Lift**: output energy lift.
- **C4 Bias**: cross‑channel blend.

### 7. Harmonic (Neural Harmonic Cartographer)
Spectral remap of bins into stretched and inharmonic grids, with gated sparsity and optional mirror folding from highs to lows.
- **C1 Stretch**: harmonic spacing scale.
- **C2 Inharm**: inharmonic bend amount.
- **C3 Sparse**: gating/sparsity of partials.
- **C4 Mirror**: fold highs into lows.

### 8. PhaseLoom (Neural Phase Loom)
Phase‑domain warp: phase swirl and tilt per bin with optional inter‑channel binding. Stereo control widens or narrows output energy.
- **C1 Bind**: lock phase between L/R.
- **C2 Swirl**: sinusoidal phase warp strength.
- **C3 Tilt**: frequency‑dependent phase skew.
- **C4 Stereo**: widen/narrow low‑bin energy.

### 9. MicroGran (Neural Micro‑Granulator)
Micro‑granulation using short delay taps with windowed holds. Drift adds time jitter (not noise). Blend crossfades dry/grain.
- **C1 Size**: grain length.
- **C2 Drift**: random time jitter.
- **C3 Blend**: dry ↔ grain.
- **C4 Scatter**: stereo offset between grain taps.

### 10. Smear (Neurotic Smear)
All‑pass pole chain with feedback for broad, phase‑smeared diffusion. LFO modulates frequency.
- **C1 Frequency**: all‑pass frequency (LFO applied).
- **C2 Resonance**: pole feedback intensity (kept stable).
- **C3 Poles**: 2–128 pole count.
- **C4 FDBK**: feedback amount.

## Notes
- LFO affects algorithms where modulation makes sense (e.g., Braid, TapeHyd, Diffusion, PhaseLoom, Binaural, Smear).
- MIX defaults to 80% wet; set to taste for parallel processing.
