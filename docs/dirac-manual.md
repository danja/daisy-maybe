# Dirac — User Manual

A time-domain granular synthesizer for the kxmx_bluemchen, ported from
[Dirac for the ER-301](https://github.com/nickb808) by Nicholas Breinich
(Apache-2.0 — see `dirac/LICENSE` and `dirac/NOTICE`).

Dirac chops a sound into short overlapping grains, each read straight from the
audio, windowed, pitched by playback speed, and layered. It granulates the live
input by default and switches to a loaded sample as soon as you pick one from
the card.

---

## Quick start

1. Patch audio into **IN L**. You'll hear a cloud immediately — density defaults
   to 3 and free-runs.
2. **Knob 1** is Playhead, **Knob 2** is Density. Both use *pickup*: a knob does
   nothing until it crosses the current stored value, so nothing jumps when you
   change pages or reassign it. Turn it until it catches.
3. `glen` sets grain size, `grns` how many overlap: short + sparse is
   pointillistic, long + dense is a smooth wash.
4. `text` shapes each grain — down for crisp percussive bits, centred for a
   rounded Hann, up for a fuller flat top.
5. `fdbk` regenerates and sustains grains after the input stops (drones, smears).
   In live mode `plyh` is the feedback delay time.
6. Put a WAV file on an SD card in `/dirac`, then **SMPL → file → press** to
   granulate it. `plyh` sweeps through it; `sped` scans it independently of pitch.
7. Long-press the encoder for the **grain field** — one glyph per grain, X =
   stereo position, Y = pitch. Press again in that view to flip X to *read
   position in the buffer*, which is the view that makes Snap legible.

---

## Controls

**Encoder** (per `docs/menu-system.md`):

| Gesture | Effect |
|---|---|
| Rotate on the title row | Change page |
| Press | Move the selection down; wraps back to the title |
| Rotate on an item | Edit it |
| Press on an action row (`file`, `detc`, `scan`) | Fire the action, then move on |
| Long press (>500 ms) | Toggle the grain field view |
| Press in the field view | Flip the X axis: pan ↔ read position |

**Knobs and CV** are assignable on the `MOD` page — any of the 31 parameters
can be a destination. Defaults:

| Control | Default destination |
|---|---|
| Knob 1 | `plyh` (playhead) |
| Knob 2 | `dens` (density) |
| CV 1 | `voct` at 100% depth (1 V/oct) |
| CV 2 | `plyh` at 50% depth |

CV is a **bipolar offset** around the knob/menu value, scaled by its depth
(−100…+100%), except when the destination is `voct` — there it is read as volts
with the tracking calibration applied, so 1 V really is an octave.

**Audio in:**

- **IN L** — always the capture source.
- **IN R** — `IO → in-R` selects what it does:
  - `capt` (default) — summed with IN L into the capture ring.
  - `trig` — edge-detected; each pulse spawns one grain. The codec inputs are
    AC-coupled so a sustained *gate* droops, but a *trigger pulse* works cleanly.
    This gives back the ER-301's `trig` input without spending a CV jack.
  - `mod` — envelope follower feeding the `MOD` destination on the MOD page.

**Audio out:** `IO → out` — `st` (stereo cloud), `w+d` (L = cloud, R = dry
input, for parallel patching), `mono`.

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

The title row shows the page name, an `S` when a sample is attached, and a
blinking heartbeat.

## Parameters

| Name | Range / default | What it does |
|---|---|---|
| `dens` | 0–16 / 3 | Target grain overlap. 0 = trigger-only (free-run off). |
| `glen` | 1 ms – 2 s / 50 ms | Grain length. Stepped logarithmically. |
| `grns` | 1–16 / 12 | Polyphony limit. At the cap, new spawns are skipped rather than cutting a playing grain. |
| `plyh` | 0–1 / 0 | **Sample:** read position. **Live:** how far back grains read — also the feedback delay. |
| `sped` | −4…+4 / 0 | Playhead scan rate; decouples time from pitch. Sample mode only. 0 = parked. |
| `semi` | −24…+24 / 0 | Coarse transpose, integer semitones. |
| `voct` | −4…+4 oct / 0 | Continuous pitch. Summed with `semi`, clamped to ±4 octaves. |
| `pspr` | 0–12 st / 0 | Per-grain pitch scatter. |
| `scal` | off / chrm / maj / min / pnt5 / pntm / whol | Quantizes the scatter and the head interval ladder to a scale. |
| `detu` | 0–2 st / 0 | Stereo detune (R against L). |
| `wind` | norm, bell, sinc, tri, decy, ramp, tuky, sqr | Grain envelope shape. `sinc` is signed — grains flip polarity mid-grain. |
| `text` | 0–1 / 0.5 | At `norm`: percussive → Hann → Tukey. In a fixed window it becomes that shape's own macro (bell width, sinc lobe count, decay time, taper…). |
| `tilt` | −1…+1 / 0 | Envelope peak position: −1 = peak at the start (decay), 0 = symmetric, +1 = swell into a hard stop. |
| `tspr` | 0–1 / 0 | Per-grain tilt scatter — attack and swell grains interleaved in one cloud. |
| `sprd` | 0–1 / 0.5 | Stereo position scatter. |
| `bin` | 0–1 / 0 | Binaural depth: interaural delay + head-shadow filtering on top of the pan. |
| `pjtr` | 0–1 / 0 | Random spray of each grain's read position (±100 ms). |
| `rev` | 0–1 / 0 | Probability a grain plays backwards. |
| `colr` | 0–1 / 0 | Per-grain random low-pass cutoff — the spectral version of `pspr`. |
| `fdbk` | 0–1 / 0 | Reinjects output into the capture ring, soft-clipped. ~0.8 = clear tail, ~1 = held drone. |
| `mix` | 0–1 / 1 | Wet/dry. Live mode only. |
| `levl` | 0–1 / 0.7 | Output level. |
| `comp` | 0–1 / 0 | Per-grain leveling (±12 dB) so sparse clouds stay even. |
| `difu` | off / on | Four cascaded allpasses per channel — smears grain edges into a halo, and feeds the regeneration loop. |
| `mult` | off / on | Reveals multi-playhead. Off hard-bypasses it. |
| `head` | 1–4 / 1 | Playheads sharing the buffer; grains dealt round-robin. |
| `hspr` | 0–1 / 0 | Position offset between heads. |
| `htun` | −12…+12 st / 0 | Interval between heads. `7` stacks fifths, `−12` octaves down. With `scal` set the ladder becomes diatonic. |
| `snap` | 0–1 / 0 | Pulls each grain's read onto the nearest transient. Turns a smeared drum loop back into hits. |
| `hold` | off / on | Freezes the current grains into a looping cloud. |
| `frez` | off / on | Freezes the capture *ring* — the source locks, every control stays live. (`hold` freezes the cloud; `frez` freezes the source.) |

---

## Samples from the SD card

Put 16/24/32-bit PCM or 32-bit float WAVs (mono or stereo, any sample rate) in
`/dirac` on a FAT-formatted card. The card root is used if there is no `/dirac`
directory. Up to 64 files are listed.

- `SMPL → file` — rotate to choose, **press to load**.
- `SMPL → detc` — press to detach and return to the live input.
- `SMPL → scan` — press to re-read the card (after a swap).
- `SMPL → len` — loaded length in tenths of a second. `sd` shows the card status
  or the reason a file was refused.

Files are mono-summed into SDRAM. Capacity is **8 M frames ≈ 2.9 minutes** at
48 kHz; longer files load truncated and `sd` reads `trunc`. Sample rate is
handled without a resampler — the file's rate folds into the grain playback
speed, so a 44.1 kHz file plays at the right pitch.

Loading blocks the UI for up to a second on a long file. **Audio keeps
running**: the engine detaches to live mode first, so nothing reads the store
while it is being rewritten.

---

## MIDI

MIDI arrives on the TRS input. `MIDI → mch` selects the channel (`0` = omni).

**Notes** — note 60 is unity pitch; the note offset is summed with `semi` and
the V/Oct CV. Velocity scales `levl`. `MIDI → note` picks the behaviour:

- `trig` (default) — each note-on fires one grain. Play a sequence, get a
  rhythm of grains.
- `gate` — the cloud free-runs only while a note is held.
- `ltch` — the note only sets pitch; the cloud runs continuously.

Pitch bend is ±2 semitones.

**Program change** loads the sample at that index from the card listing.

**Clock** — `MIDI → clk` set to anything but `off` spawns a grain on the beat
division (1/1 … 1/32) derived from incoming MIDI clock, following start/stop.

**CC map** — every parameter is reachable. The defaults are the parameter table
in page order from CC 16, plus the conventional three:

| CC | Parameter |
|---|---|
| 1 | `plyh` (mod wheel) |
| 7 | `levl` (channel volume) |
| 64 | `hold` (sustain pedal) |
| 16–46 | Parameters in table order: 16 `dens`, 17 `glen`, 18 `grns`, 19 `plyh`, 20 `sped`, 21 `semi`, 22 `voct`, 23 `pspr`, 24 `scal`, 25 `detu`, 26 `wind`, 27 `text`, 28 `tilt`, 29 `tspr`, 30 `sprd`, 31 `bin`, 32 `pjtr`, 33 `rev`, 34 `colr`, 35 `fdbk`, 36 `mix`, 37 `levl`, 38 `comp`, 39 `difu`, 40 `mult`, 41 `head`, 42 `hspr`, 43 `htun`, 44 `snap`, 45 `hold`, 46 `frez` |

**CC learn:** select a parameter row, set `MIDI → cc` to `arm`, then move a
control on your controller — that CC binds to the selected parameter and `cc`
returns to `off`.

Events are drained in the main loop, so trigger timing is quantised to one
audio block (1 ms) rather than being sample-accurate.

---

## V/Oct calibration

`IO → cscl` (tracking, per mille) and `IO → cofs` (offset, cents) tune the
1 V/oct response, in the same spirit as the resonators firmware. Patch a known
pitch source, play octaves, and adjust `cscl` until they track; use `cofs` to
put it in tune. Values are saved to QSPI automatically a second after the last
edit, so a calibrated module stays calibrated.

---

## Reading the grain field

Long-press to enter. Each active grain is one glyph.

| You see | It means |
|---|---|
| Horizontal position | Stereo placement — or read position, after a press |
| Vertical position | Pitch (±24 semitones across the field) |
| Stem under the glyph | Grain length — long grains stand tall |
| Glyph hanging downward | Reverse grain |
| Lingering trail | The grain's envelope decaying (persistence, since the panel is 1-bit) |
| Header | Axis mode and the live grain count |

---

## Notes and known behaviour

**Snap's hit rate depends on the material.** The coarse search stage tests 12
candidates across ±75 ms, but the energy-rise score is only positive over
roughly the 240 samples leading into a transient. So stage 1 finds an onset
about a third of the time and the rest spawn unsnapped. Measured on a synthetic
click track, `snap 1` lands 1.7–1.8× closer to onsets than `snap 0`. This
geometry is identical to the ER-301 original — it is a property of the
algorithm, not of the port.

**Knob pickup at power-up.** Both knobs start inert and take over only when they
cross the stored value. This is deliberate: without it, every page change or
reassignment would jump the sound to wherever the knob happens to be sitting.

**Flash is full.** The firmware sits at 99.9% of the 128 KB internal flash. If
you add features, switch the Makefile to `APP_TYPE = BOOT_QSPI` (needs the
Electrosmith bootloader flashed once) for 8 MB of room. See the notes in
`dirac/Makefile`.

---

## Build

```bash
make -C dirac
make -C dirac program-dfu      # hold BOOT, tap RESET, release BOOT
```

Host-side DSP tests (no hardware needed):

```bash
make -C host_dsp dirac_fixture && ./host_dsp/dirac_fixture all
make -C host_dsp dirac_asan    && ./host_dsp/dirac_asan all   # ASan + UBSan
./host_dsp/dirac_fixture cpu                                   # relative cost
```
