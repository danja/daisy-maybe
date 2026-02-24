# Wavelet Shimmer Ladder Plan

## Goal
Add a new Neurotic algorithm: **Wavelet Shimmer Ladder Lite**.

Design target:
- Shimmer-like upward spectral motion.
- Stable on Daisy Seed CPU/RAM budget.
- No dynamic allocation in audio callback.

## Constraints
- Keep callback allocation-free.
- Keep per-sample work bounded and deterministic.
- Favor fixed-size buffers and simple math.
- Avoid large FFT-style windows.

## Proposed DSP Design

### Transform Strategy
- Use fixed-block **Haar wavelet** decomposition.
- Block size: start with `N=32` (fallback to `N=64` if artifacts are too strong).
- Levels: 3 max.
- Effective bands:
  - `A3` coarse band
  - `D3` mid band
  - `D2 + D1` fine/high bands

### Shimmer Ladder Operation
Per block, after decomposition:
1. Compute upward transfer:
   - Move a controlled fraction of lower-band energy into the next higher band.
2. Apply damping per band.
3. Apply ladder feedback on transformed bands (bounded and soft-clipped).
4. Reconstruct block with inverse Haar.
5. Crossfade reconstructed output with previous block edge to suppress clicks.

### Stability Controls
- Clamp feedback to conservative range (e.g. `0.0..0.75`).
- Soft-clip internal feedback paths.
- DC suppression after reconstruction (simple high-pass one-pole).

## Parameter Mapping
- `C1` Ladder amount: how strongly energy is pushed upward.
- `C2` Feedback: global shimmer feedback depth.
- `C3` Damping/Tilt: high values damp fine bands more.
- `C4` Diffusion: random/phase decorrelation amount per block.
- Global `Mix`: dry/wet blend.
- Global `Feed`: kept as framework feedback unless conflicts; if conflict occurs, route shimmer-internal feedback to `C2` only.

## Implementation Steps

1. Add algorithm shell
- Add new algorithm class in `neurotic/algos/neurotic_algos.cpp`.
- Register it in bank init/reset/process switch.

2. Add fixed buffers/state
- Input/output block buffers per channel.
- Wavelet coefficient buffers for 3 levels.
- Crossfade and DC filter states.

3. Implement Haar forward/inverse
- Fixed loops, no heap allocation.
- Unit-test-like offline checks in host simulation (impulse, sine, noise).

4. Implement ladder processing
- Band transfer matrix.
- Damping + bounded feedback path.
- Soft clipping for stability.

5. Integrate UI mapping
- Add parameter labels for the new algorithm page.
- Keep menu item lengths within current display constraints.

6. Tune for hardware
- Start conservative defaults.
- Verify no overload/noise bursts with max settings.
- Measure subjective quality on transients and harmonic sources.

## Verification Plan
- Build: `make -C neurotic`.
- Audio sanity checks:
  - `Mix=0` dry unaffected.
  - `Mix=100` wet audible, no silence.
  - Feedback sweep remains stable.
- Edge tests:
  - Low input level.
  - High transient content.
  - Rapid knob/CV changes.

## Risks and Mitigations
- Clicking at block boundaries:
  - Mitigate with short overlap/crossfade.
- Metallic artifacts:
  - Increase damping and reduce ladder gain.
- CPU spikes:
  - Keep `N=32`, fixed 3-level transform, no expensive trig.

## Deliverable Scope (Phase 1)
- Single stable shimmer ladder algorithm.
- No per-scale UI page yet.
- Parameter behavior documented in `docs/neurotic-manual.md` after tuning.
