# Dirac — Quick Reference

Granular synthesizer for the kxmx_bluemchen. Chops sound into short overlapping
grains — each read from the audio, windowed, pitched, and layered. Granulates the
live input by default; switches to a card sample when you load one.

Full details: `docs/dirac-manual.md`.

---

## Start here

1. Patch audio into **IN L** — you'll hear a cloud straight away.
2. **Knob 1** = playhead, **Knob 2** = density. Both use *pickup*: a knob is inert
   until it crosses the stored value. Turn until it catches.
3. `glen` = grain size, `grns` = how many overlap. Short + sparse = pointillistic;
   long + dense = smooth wash.
4. `fdbk` sustains the cloud after the input stops (drones, smears).
5. Long-press the encoder for the **grain field** display.

---

## Controls

| Gesture | Effect |
|---|---|
| Rotate on title row | Change page |
| Press | Move selection down (wraps to title) |
| Rotate on item | Edit it |
| Press on `file` / `detc` / `scan` | Fire that action |
| Long press (>500 ms) | Toggle grain field view |
| Press in field view | Flip X axis: pan ↔ read position |

**Knobs / CV** — assignable on the `MOD` page, any of 31 destinations.
Defaults: K1 → `plyh`, K2 → `dens`, CV1 → `voct` (100%), CV2 → `plyh` (50%).
CV is a bipolar offset around the knob value, except on `voct` where it is read
as volts with calibration applied.

**IN L** — always the capture source.
**IN R** — set by `IO → in-R`: `capt` (sum into ring, default) / `trig` (pulse
spawns a grain) / `mod` (envelope follower → MOD destination).

**Out** — `IO → out`: `st` stereo / `w+d` wet+dry / `mono`.

---

## Pages

| Page | Items |
|---|---|
| `GRAIN` | dens, glen, grns, plyh, sped |
| `PITCH` | semi, voct, pspr, scal, detu |
| `SHAPE` | wind, text, tilt, tspr |
| `SPACE` | sprd, bin, pjtr, rev, colr |
| `FDBK` | fdbk, mix, levl, comp, difu |
| `HEADS` | mult, head, hspr, htun, snap |
| `SMPL` | file, frez, hold, detc, scan, len, sd |
| `MOD` | K1, K2, CV1, cv1d, CV2, cv2d, MOD, modd |
| `MIDI` | mch, note, clk, cc |
| `IO` | in-R, out, cscl, cofs |
| `SYS` | cpu, grn, len, ver |

Title row shows the page, `S` when a sample is attached, and a heartbeat.

---

## Parameters

| Name | Range / default | What it does |
|---|---|---|
| `dens` | 0–16 / 3 | Grain overlap. 0 = trigger-only. |
| `glen` | 1 ms–2 s / 50 ms | Grain length. |
| `grns` | 1–16 / 12 | Polyphony cap. |
| `plyh` | 0–1 / 0 | Read position. In live mode, also the feedback delay. |
| `sped` | −4…+4 / 0 | Playhead scan rate (sample mode). |
| `semi` | −24…+24 / 0 | Coarse transpose. |
| `voct` | −4…+4 oct / 0 | Continuous pitch. |
| `pspr` | 0–12 st / 0 | Per-grain pitch scatter. |
| `scal` | off/chrm/maj/min/pnt5/pntm/whol | Quantize scatter + head intervals. |
| `detu` | 0–2 st / 0 | Stereo detune. |
| `wind` | norm, bell, sinc, tri, decy, ramp, tuky, sqr | Grain envelope. |
| `text` | 0–1 / 0.5 | Envelope macro: percussive → Hann → Tukey. |
| `tilt` | −1…+1 / 0 | Envelope peak: decay ← symmetric → swell. |
| `tspr` | 0–1 / 0 | Per-grain tilt scatter. |
| `sprd` | 0–1 / 0.5 | Stereo scatter. |
| `bin` | 0–1 / 0 | Binaural depth. |
| `pjtr` | 0–1 / 0 | Read-position spray (±100 ms). |
| `rev` | 0–1 / 0 | Reverse-grain probability. |
| `colr` | 0–1 / 0 | Per-grain low-pass scatter. |
| `fdbk` | 0–1 / 0 | Regeneration. ~0.8 tail, ~1 drone. |
| `mix` | 0–1 / 1 | Wet/dry (live mode). |
| `levl` | 0–1 / 0.7 | Output level. |
| `comp` | 0–1 / 0 | Per-grain leveling (±12 dB). |
| `difu` | off/on | Allpass diffusion halo. |
| `mult` | off/on | Enable multi-playhead. |
| `head` | 1–4 / 1 | Playheads, round-robin. |
| `hspr` | 0–1 / 0 | Offset between heads. |
| `htun` | −12…+12 st / 0 | Interval between heads. |
| `snap` | 0–1 / 0 | Pull grain reads onto transients. |
| `hold` | off/on | Freeze the cloud. |
| `frez` | off/on | Freeze the source ring. |

---

## SD card

16/24/32-bit PCM or float WAV, mono or stereo, any rate, in `/dirac` (or the card
root). Up to 64 files.

`SMPL → file` rotate + **press to load** · `detc` back to live · `scan` re-read
card · `len` loaded length · `sd` card status.

Capacity **≈2.9 minutes**; longer files truncate (`sd` reads `trunc`). No
resampler needed — file rate folds into playback speed. Audio keeps running
during a load.

---

## MIDI

TRS input. `MIDI → mch` sets channel (`0` = omni). Note 60 = unity; velocity
scales `levl`. Pitch bend ±2 st.

`MIDI → note`: `trig` (note fires a grain, default) / `gate` (cloud runs while
held) / `ltch` (note sets pitch only).

**Program change** loads the sample at that index.
**Clock** — `MIDI → clk` spawns on a beat division (1/1…1/32) from MIDI clock.

**CC:** 1 → `plyh`, 7 → `levl`, 64 → `hold`; 16–46 map to the parameter table
above in order (16 `dens` … 46 `frez`). To learn: select a row, set `MIDI → cc`
to `arm`, move a control.

Timing is quantised to one audio block (~1 ms).

---

## Grain field

Long-press to enter. One glyph per grain.

X = pan (or read position after a press) · Y = pitch (±24 st) · stem = grain
length · hanging down = reverse · trail = envelope decay · header = axis mode and
grain count.

---

## V/Oct calibration

`IO → cscl` (tracking) and `IO → cofs` (offset). Play octaves, adjust `cscl`
until they track, `cofs` to tune. Saved to QSPI a second after the last edit.
