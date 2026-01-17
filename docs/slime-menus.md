# Slime menus

This document lists the current menu pages and what each page controls in the Slime firmware.

## Page order

1. PRC: Process selection (Thru, Smear, Shift, Comb, Freeze, Gate, Tilt, Fold, Phase)
2. R: Time ratio (time2 / time1)
3. MX: Wet/dry mix
4. BYP: Bypass (hard bypass in audio callback)
5. PR: Preserve amount (blend original spectrum back in)
6. SP: Spectral gain (pre-IFFT gain)
7. IF: IFFT gain (post-IFFT gain)
8. OL: Overlap-add gain
9. WIN: Window selection (SQH, HAN, BHS, SIN, REC, KAI)
10. KB: Kaiser beta (only affects KAI window)
11. PH: Phase continuity toggle
12. WCL: Wet clamp mode (0=off, 1=soft, 2=hard)
13. NRM: Spectrum normalization toggle
14. LIM: Spectrum magnitude limiter toggle
15. IN: Input meters (IN, CL, OT)
16. WET: Wet/mix meters (WT, M1, M2)
17. CPU: CPU load meters (LD, MS, BD)
18. KNB: Raw ADC reads (K1, K2, C)

## Notes

- Time ratio adjusts time2 relative to time1; time1 is controlled by knob + CV 1, time2 by ratio.
- Process and window pages wrap around.
- The meters are in the same order as the pages above.
