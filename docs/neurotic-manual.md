# Neurotic Manual

Neurotic is a 17‑algorithm processor for the kxmx_bluemchen hardware. Each algorithm has its own page and name at the top line. The **title line** is the algorithm selector (rotate encoder to change). Below the title are shared controls plus two or three algorithm‑specific parameters. Most algorithms keep low and mid control settings playable, then push into more extreme behavior near the top of the control range.

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
7. **Param 3 (C5)** – Algorithm‑specific (only on some algorithms).

## Algorithms

### 0. CrossRes (Cross‑Resonator)
Two cascaded SVF band‑pass resonators per channel, followed by a one‑pole damping filter. The resonant center follows tension; stereo offset introduces detune between channels.
- **C1 Tension**: base resonant frequency (musical scale).
- **C2 Mass**: increases resonator spacing (modal density/spread).
- **C3 Damp**: damping amount (darker/shorter as it rises).
- **C4 Asym**: stereo detune between L/R resonators.
- **C5 Res**: resonator Q (higher = sharper).

### 1. Braid (Latent Spectral Braider)
STFT cross‑spectral braid: magnitudes and phases of L/R are interwoven per‑bin with transient protection. FFT is 1024 with 256‑sample hop and windowed overlap/add.
- **C1 Braid Depth**: strength of L/R magnitude exchange.
- **C2 Formant Swap**: bias of envelope sharing between channels.
- **C3 FORM**: transient protection (higher keeps attacks intact).
- **C4 TRANS**: additional weave emphasis across bands.

### 2. TapeHyd (Tape Hydraulics)
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
- **C4 Spin**: rotation amount (driven by global LFO).

### 4. Formant (Formant Forge)
Three SVF band‑pass formant filters per channel morph through vowel anchors in a two‑axis vowel plane. The anchors are A/E/I/O/U style formant sets; moving C1 and C2 blends smoothly between them instead of simply scaling pitch. ARTIC injects noise into the excitation. BREATH adds “air” (high‑passed input) and stereo formant divergence.
- **C1 Vowel X**: horizontal vowel morph axis.
- **C2 Vowel Y**: vertical vowel morph axis.
- **C3 ARTIC**: noise injection amount (default 0).
- **C4 BREATH**: air mix and stereo divergence.
- **C5 Res**: formant resonance (Q).

### 5. Diffusion (Diffusion Multiband)
Dual short delays with sinusoidal modulation and feedback smear. Color tilts dry contribution for a spectral skew.
- **C1 Spread**: delay depth/spread.
- **C2 Color**: spectral tilt between dry and delayed.
- **C3 Grain**: feedback amount in the diffusers.
- **C4 Drift**: modulation rate/depth.

### 6. Harmonic (Harmonic Cartographer)
Spectral remap of bins into stretched and inharmonic grids, with gated sparsity and optional mirror folding from highs to lows.
- **C1 Stretch**: harmonic spacing scale.
- **C2 Inharm**: inharmonic bend amount.
- **C3 Sparse**: gating/sparsity of partials.
- **C4 Mirror**: fold highs into lows.

### 7. PhaseLoom (Phase Loom)
Phase‑domain warp: phase swirl and tilt per bin with optional inter‑channel binding. Stereo control widens or narrows output energy.
- **C1 Bind**: lock phase between L/R.
- **C2 Swirl**: sinusoidal phase warp strength.
- **C3 Tilt**: frequency‑dependent phase skew.
- **C4 Stereo**: widen/narrow low‑bin energy.

### 8. Smear (Neurotic Smear)
All‑pass pole chain with feedback for broad, phase‑smeared diffusion. LFO modulates frequency.
- **C1 Frequency**: all‑pass frequency (LFO applied).
- **C2 Resonance**: pole feedback intensity (kept stable).
- **C3 Poles**: 2–128 pole count.
- **C4 FDBK**: feedback amount.

### 9. PitchShift (Pitch Shifter)
Dual‑window delay pitch shifter with musical offset.
- **C1 Pitch**: musical pitch offset (continuous, bipolar).
- **C2 Scale**: scales the offset depth.
- **C3 Win**: window length (quality vs latency).
- **C4 Stereo**: slight L/R spread.

### 10. Reverb (Neurotic Reverb)
Two‑delay reverb with prime‑spaced taps and cross‑feedback. Overall delay time is log‑scaled.
- **C1 Time**: overall delay time (0.125×–4×, log scale).
- **C2 Feedback**: feedback amount.
- **C3 Cross**: cross‑feed between delay lines.
- **C4 Taps**: 1–10 taps at prime intervals.
- **C5 Tilt**: emphasize longer vs shorter taps.

### 11. EuDelay (Euclidean Delay)
Delay taps spaced using Euclidean rhythm. Taps and offsets follow the shared C1/C2 control path (Knob + CV bipolar mapping).
- **C1 Taps**: number of taps (1–Steps).
- **C2 Offset**: tap offset in steps (0–Steps‑1).
- **C3 Steps**: number of steps (2–16).
- **C4 BPM**: tempo (80–200).
- **C5 Scale**: ratio of delay time to BPM.
- **Feed**: global feedback amount for EuDelay (same as other algorithms).
- **Implementation note**: internal delay memory runs at 8x downsample for longer effective delay time.

### 12. WavScram (Wavelet Scale Scrambler)
Block-based Haar wavelet remap that scrambles coarse/mid/fine bands with smoothing.
- **C1 Amount**: scramble depth (identity -> fully remapped).
- **C2 Rate**: permutation change rate.
- **C3 Smth**: smoothing of coefficient updates (higher = smoother).
- **C4 Offs**: stereo offset between left/right remap.
- **C5 Tilt**: coarse/fine spectral tilt after remap.

### 13. ModalBank (Rings Modal Bank)
Rings-inspired modal resonator bank using twelve stereo SVF band-pass modes. Structure bends the modal spacing from compressed/inharmonic to stretched, while position changes the pickup balance between modes.
- **C1 Root**: fundamental/modal root frequency.
- **C2 Structure**: modal stiffness and spacing.
- **C3 Bright**: high-mode brightness.
- **C4 Damp**: modal resonance/decay.
- **C5 Pos**: pickup position across the modal bank.

### 14. SympString (Rings Sympathetic Strings)
Three lightweight Karplus strings tuned to sympathetic chord relationships. Input excites the main string and nearby sympathetic strings, with stereo spread across the bank.
- **C1 Root**: root string frequency.
- **C2 Chord**: chord/spread family.
- **C3 Damp**: feedback/decay time.
- **C4 Bright**: damping filter brightness and excitation.
- **C5 Detn**: small detune/dispersion amount.

### 15. FMRes (Rings FM Voice)
Two-operator FM voice inspired by Rings' FM mode, with the input envelope driving FM amount and output level.
- **C1 Carrier**: carrier pitch.
- **C2 Ratio**: modulator ratio.
- **C3 Bright**: FM index/brightness.
- **C4 FB**: feedback character.
- **C5 Damp**: envelope decay and tone damping.

### 16. Bazzer (Crossover + Sub + Mid Drive)
Stereo crossover processor with octave-down bass reinforcement, high-band enhancement, and band-passed distortion.
- **C1 BassMix**: level of octave-down low-band reinforcement (both channels).
- **C2 HighMix**: level of added high band (both channels).
- **C3 Xovr**: crossover frequency for low/high split (50–250 Hz).
- **C4 Mid**: center frequency of broad mid band-pass feeding distortion.
- **C5 Drive**: tanh distortion drive and blend for the mid band.
- **Mod/LFO**: modulates a stereo all-pass phase shift on the high band for moving width (with opposite L/R phase motion).

## Notes
- LFO affects algorithms where modulation makes sense (e.g., Braid, TapeHyd, Diffusion, PhaseLoom, Binaural, Smear).
- MIX defaults to 80% wet; set to taste for parallel processing.
