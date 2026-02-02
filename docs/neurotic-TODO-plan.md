# Neurotic TODO - Implementation Plan

1. **Menu display correctness**
   - Audit menu formatting for Percent/Hz/Int to ensure zeros render explicitly.
   - Align LFO rate display formatting with resonators if needed.

2. **Parameter ownership & duplication**
   - Ensure menu/encoder parameters write to the same state variables that DSP reads (no parallel duplicate storage).
   - Only keep temporary values where conversion is required (e.g., integer display for Smear poles).

3. **Add additional per‑algorithm menu parameters**
   - Extend Neurotic state/runtime with an extra algorithm parameter (C5).
   - Expand UI to include a 3rd algorithm-specific menu item when required.
   - Default C5 to a sensible midpoint (or 0.0) and update docs.

4. **Algorithm fixes & control mapping**
   - **CrossRes**: swap Mass/Tension so pitch is on C1; add Resonance param (menu).
   - **Braid**: reduce crunch/artifacts (tame bin mixing, add magnitude smoothing or gain staging).
   - **Binaural**: move Spin modulation to LFO rate/depth; make C4 control amount only.
   - **Formant**: add Resonance param (menu).
   - **Harmonic**: raise output and reduce harsh distortion.
   - **PhaseLoom**: make C1/C2/C3 audible; verify LFO binding.
   - **Smear**: ensure pitch uses only C1 and resonance only C2.

5. **New algorithms**
   - **Compressor/Expander**: add algorithm with C1 amount, C2 time‑constant scale; menu Attack/Decay scaled by time‑constant.
   - **Pitch Shifter**: add algorithm with C1 pitch offset (musical, bipolar), C2 scaling; menu for range/quality if needed.

6. **Docs & labels**
   - Update `docs/neurotic-manual.md` and `docs/neurotic-smear.md` to reflect new params, counts, and labels.

