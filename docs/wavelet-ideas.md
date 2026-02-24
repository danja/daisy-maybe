# Wavelet Algorithm Ideas for Neurotic

## Idea List

### 1. Fractal Freeze
- Hold selected wavelet scales while letting others pass.
- Produces textured freeze effects with partial motion.
- Suggested controls:
  - `C1` Freeze depth
  - `C2` Release speed
  - `C3` Scale focus (fine -> coarse)
  - `C4` Stereo decorrelation

### 2. Transient Relocator
- Detect transient energy in fine scales and re-emit it later in coarser scales.
- Turns attacks into ghost echoes and spectral trails.
- Suggested controls:
  - `C1` Transient sensitivity
  - `C2` Relocation delay
  - `C3` Destination scale
  - `C4` Density

### 3. Wavelet Shimmer Ladder
- Shift energy upward across scales with controlled feedback.
- More natural than typical FFT shimmer, less metallic.
- Suggested controls:
  - `C1` Upward shift amount
  - `C2` Feedback
  - `C3` Per-scale damping
  - `C4` Diffusion

#### Hardware-constrained variant: Wavelet Shimmer Ladder Lite
- Use a short block Haar transform (32 or 64 samples), 3 levels max.
- Limit to 3 effective bands (coarse, mid, fine) to keep CPU predictable.
- Move only a fraction of energy upward each block (no full resynthesis tricks).
- Clamp feedback and use per-band damping to prevent unstable ringing.
- Use small crossfades between blocks to avoid zipper/clicks.

### 4. Scale Scrambler
- Rotate or permute scale bands per block with smoothing.
- Preserves timing while mutating timbre.
- Suggested controls:
  - `C1` Scramble amount
  - `C2` Change rate
  - `C3` Smoothing
  - `C4` Stereo offset

### 5. Wavelet Vocoder Lite
- Use one channel as scale-envelope modulator for the other.
- Voice-like articulation with lower smear than FFT vocoding.
- Suggested controls:
  - `C1` Modulation amount
  - `C2` Attack/release
  - `C3` Scale tilt
  - `C4` Intelligibility/noise blend

### 6. Micro-Doppler Warp
- Modulate per-scale phase/time alignment using LFO/CV.
- Elastic chorus/flange that adapts to source texture.
- Suggested controls:
  - `C1` Warp depth
  - `C2` Rate
  - `C3` Fine/coarse emphasis
  - `C4` Feedback

### 7. Wavelet Gate Matrix
- Independent threshold gates per scale with optional Euclidean masks.
- Rhythmic spectral chopping that tracks input dynamics.
- Suggested controls:
  - `C1` Threshold
  - `C2` Rhythm density
  - `C3` Scale weighting
  - `C4` Release

### 8. Entropy Painter
- Compute per-scale entropy/variance.
- Map low-entropy bands to distortion and high-entropy bands to smoothing.
- Suggested controls:
  - `C1` Bias
  - `C2` Contrast
  - `C3` Drive
  - `C4` Smoothing
