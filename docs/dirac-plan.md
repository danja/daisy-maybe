# Dirac (bluemchen) — Implementation Plan

Port of the ER-301 granular synthesizer [er301-Dirac](https://github.com/…/er301-Dirac)
(`~/github/er301-Dirac`, Apache-2.0, © Nicholas Breinich) to the kxmx_bluemchen /
Daisy Seed, as a new `dirac/` firmware in this repo.

Source of truth for the algorithm: `er301-Dirac/src/Dirac.cpp` (1000 lines) and
`src/Dirac.h` (368 lines). Everything below assumes we keep that DSP essentially
intact and replace the ER-301 host layer.

---

## 1. What we are porting

The ER-301 unit is a time-domain overlap-add granulator: a 16-slot grain pool,
each grain reading the source with linear interpolation, windowed by a table
envelope, pitched by playback speed, panned equal-power, summed to stereo.

Feature inventory, with a portability verdict for each:

| Feature | Where | Port |
|---|---|---|
| Grain pool, spawn scheduling (density free-run + trigger edges) | `spawnGrain`, `process` | verbatim |
| Live capture ring (131072 = 2.73 s) + feedback with tanh + 120 Hz loop HP | `process` | verbatim, ring → SDRAM |
| Sample mode: read a loaded buffer at a playhead | `readHoisted` | rewrite the `od::Sample` accessor only |
| Envelope tables: perc/Hann/Tukey morph + 7 fixed windows × 5 variants | `buildEnvTables` | verbatim, but shrink table size (§4) |
| Tilt / TiltSpread (envelope peak position, two-rate phase) | `spawnGrain`, render loop | verbatim |
| Pitch: SemiShift + V/Oct + Psprd + scale quantization | `snapToScale` | verbatim |
| Multi-playhead (heads / hsprd / htune) | `spawnGrain` | verbatim |
| Snap (coarse→fine onset search, budgeted 2/block) | `spawnGrain` | verbatim |
| Compress (spawn-time RMS peek + makeup) | `spawnGrain` | verbatim |
| Color (per-grain LP cutoff scatter) | `spawnGrain`, render | verbatim |
| Binaural (ITD + head shadow) | `spawnGrain`, render | verbatim |
| Reverse, PosJtr, Spread, Detune, Hold | — | verbatim |
| Diffuse (4 Schroeder allpasses/channel) | `process` | verbatim (~9 KB of buffers) |
| Speed (playhead scan / time-stretch) | `process` | verbatim |
| `sanitize` / `softLimit` output guards | — | verbatim |
| Grain-field visualiser (42 px panel, phosphor) | `DiracFieldGraphic.h` | **redesign** for 64×32 OLED (§7) |
| `od::Inlet`/`od::Option` port plumbing | `Dirac.h` | **replace** with a params struct |
| Sample select from card/pool, buffer edit | ER-301 firmware | **replace** with our own SD loader (§6) |

The DSP is already free of ER-301 dependencies apart from three touch points:
`od::Inlet::buffer()[0]` block-rate parameter reads, `mpSample->mpData /
mSampleCount / mChannelCount`, and `FRAMELENGTH`. That is the whole port surface.

---

## 2. Target hardware — what actually constrains this

Verified against `kxmx_bluemchen/src/kxmx_bluemchen.{h,cpp}` and libDaisy:

- **OLED is 64×32**, not 128×64. (`SSD130xI2c64x32Driver`; `CLAUDE.md` is wrong
  about this and should be corrected.) With `Font_6x8` that is **10 chars × 4 rows** —
  a title row plus three parameter rows. This is the single biggest UI constraint
  against a ~28-parameter unit.
- **Controls:** 2 knobs (`CTRL_1/2`, unipolar), 2 CV in (`CTRL_3/4`, initialised
  bipolar/flipped), encoder + switch. No gate input, no buttons.
- **MIDI:** `hw.midi` is a `MidiUartHandler`, already initialised by `Bluemchen::Init()`.
  No firmware in this repo uses it yet — this is new ground here.
- **SD:** `hw.sd` is an `SdmmcHandler`, initialised with `Defaults()` by
  `Bluemchen::Init()`. FatFS is *not* linked in by default — needs `USE_FATFS = 1`
  in the Makefile, which adds `ff.c` etc. and defines `FILEIO_ENABLE_FATFS_READER`.
  The app must then create a `daisy::FatFSInterface`, `f_mount` its `GetSDPath()`.
- **Memory:** 64 MB SDRAM via `DSY_SDRAM_BSS` (`libDaisy/src/dev/sdram.h`), 512 KB
  AXI SRAM for normal `.bss`. No firmware in this repo uses SDRAM yet — Dirac will
  be the first.
- **Flash:** default `APP_TYPE = BOOT_NONE` gives **128 KB internal flash**. `uzi`
  already needs `OPT = -Os` to fit. Dirac + FatFS + a WAV parser is a real risk here
  (§9).
- **Caveat from `libDaisy/src/sys/fatfs.h`:** SDMMC1 DMA can only reach AXI SRAM,
  and the `FatFSInterface` object plus any read buffer must live there — not
  DTCMRAM, not SDRAM. So: `f_read` into a small AXI-SRAM staging buffer, then copy
  into the SDRAM sample store. Do **not** `f_read` directly into `DSY_SDRAM_BSS`.

---

## 3. Module layout

Follow the `uzi`/`neurotic` structure (app / dsp / params / ui / state split), which
is the current house pattern:

```
dirac/
  Makefile              USE_FATFS=1, OPT=-Os, gnu++17
  main.cpp              audio callback trampoline + main loop
  dirac_app.{h,cpp}     lifecycle, main loop, heartbeat, load orchestration
  dirac_engine.{h,cpp}  THE PORT — platform-free granular DSP (from Dirac.cpp)
  dirac_params.h        DiracParams POD: one field per ER-301 inlet
  dirac_state.h         UI/edit state + runtime mirror (uzi_state.h pattern)
  dirac_controls.cpp    knob/CV read, mod matrix, smoothing
  dirac_midi.{h,cpp}    note/CC/clock/program-change handling
  dirac_sample.{h,cpp}  SD scan, WAV parse, SDRAM sample store
  dirac_ui.{h,cpp}      menu pages + grain-field view
  menu_system.{h,cpp}   copied from resonators/uzi
  encoder_handler.{h,cpp}  copied
  display.{h,cpp}       64×32 rendering
```

`dirac_engine` must compile on the host with no libDaisy include — that is what
makes the test harness (§8) possible.

### Engine interface

```cpp
struct DiracSample {            // what the engine reads in sample mode
    const float *data = nullptr;  // interleaved, nullptr = live mode
    int frames = 0;
    int channels = 1;
};

class DiracEngine {
public:
    void Init(float sampleRate, float *capBuf, int capBufSize);  // caller owns the ring
    void SetSample(const DiracSample &s);                        // {} to detach
    void Process(const float *inL, const float *inR,
                 float *outL, float *outR, int n,
                 const DiracParams &p);
    // viz accessors: keep the ER-301 set verbatim (getGrainPan/Pitch/Envelope/…)
};
```

`DiracParams` is a plain struct with one float/int/bool per ER-301 inlet, filled
once per block by `dirac_controls` + `dirac_midi`. This deletes the entire
`od::Inlet` layer with no behavioural change: the ER-301 code only ever reads
`buffer()[0]` (block-rate) for every inlet except `Fire`, which is per-sample.

**`Fire` needs a decision.** ER-301 scans a gate buffer for rising edges. The
bluemchen has no gate input, so triggers come from (a) MIDI note-on, (b) an audio
input used as a trigger (edge detect on IN2, see §5), (c) internal clock / MIDI
clock. Keep the per-sample edge scan and synthesise a trigger buffer, so
sample-accurate MIDI-note timing survives.

---

## 4. Memory plan

| Buffer | ER-301 size | Bluemchen | Where |
|---|---|---|---|
| Capture ring | 131072 float = 512 KB | 262144 float = 1 MB (5.46 s) | SDRAM |
| Envelope tables | 38 × 1025 float = 156 KB | 38 × 513 float = **78 KB** | AXI SRAM |
| Diffuser allpass | ~9 KB | same | AXI SRAM |
| Sample store | host-managed | 16 MB float mono (≈5.8 min @48k) | SDRAM |
| SD staging | — | 8–16 KB | AXI SRAM |

Notes:

- **Envelope tables stay in SRAM.** They are read twice per sample per grain
  (two-table lerp); SDRAM latency in the hot loop is the one thing that would
  actually hurt. Halving `kEnvTableSize` 1024 → 512 costs nothing audible (the
  table is interpolated anyway) and brings the bank to 78 KB.
- **Enlarge the ring to 5.46 s** — SDRAM is free and the ER-301's 2.73 s was a host
  constraint. Must stay a power of two: the render loop uses `& (kCapBufSize-1)`.
- **Sample store as mono float32.** The engine's `readHoisted` already mono-sums
  stereo samples on every read; pre-summing at load time makes the hot path
  cheaper and halves the memory. Cost: no stereo-source granulation. (If stereo
  source is wanted later, store interleaved and keep the ER-301 read path — the
  code already handles `nc == 2`.)
- Sample memory is a fixed `DSY_SDRAM_BSS` array, not allocated. No `malloc`
  anywhere.

---

## 5. Controls — "maximal use of ins, outs, CV and knobs"

28 parameters, 2 knobs, 2 CV. The answer is an **assignable modulation matrix**,
not a fixed mapping.

### Knobs

Both knobs are **assignable**, with a **catch/pickup** rule so switching
assignment (or menu page) does not jump the value: the knob is inert until it
crosses the current stored value. Default: **K1 = Playhead, K2 = Density**.

Assignment lives on a `MOD` menu page; turning the encoder on the `K1:`/`K2:` row
cycles the destination parameter.

### CV inputs

Both CV inputs are **assignable** with a bipolar depth (−1…+1), summed onto the
knob/menu value and clamped to the parameter range. Follow the resonators
convention: bipolar mapping with a stored offset, calibratable.

Defaults: **CV1 = V/Oct** (with the 1 V/oct calibration path already used by
`resonators` — reuse `resonators/main.cpp`'s scale/offset calibration and
`libDaisy`'s `VoctCalibration.h`), **CV2 = Playhead**.

Sensible destinations (all should be selectable, these are just the useful ones):
V/Oct, Playhead, Density, GrainLen, Grains, Psprd, PosJtr, Spread, Snap, Color,
Tilt, TiltSpread, Window, Texture, Feedback, Mix, Level, RevProb, HeadSpread,
HeadTune, Speed, Detune, Binaural, Compress, Scale.

### Audio inputs

This is where the bluemchen can beat the ER-301 per-knob, so use both:

- **IN L** — live capture source (always).
- **IN R** — mode-selectable on the `IO` page:
  1. **Stereo capture partner** — sum L+R into the ring (default; matches ER-301,
     which granulates a mono source).
  2. **Trigger** — edge detector (rising slope over a threshold, ~5 ms lockout)
     feeds the `Fire` trigger buffer. AC-coupled codec inputs mean a *gate* will
     droop, but a **trigger pulse works fine**, and this restores the ER-301's
     `trig` inlet without spending a CV input.
  3. **Modulator** — a third audio-rate CV source into the mod matrix (envelope
     follower → any destination). Useful for audio-rate density/pitch chaos.

### Audio outputs

Stereo out is the grain cloud, exactly as ER-301 (`outL`/`outR`, soft-limited).
Add an `IO`-page **output mode**: `stereo` (default) / `wet+dry` (L = wet, R = dry
passthrough, for parallel patching) / `dual-mono`.

### Encoder

Per `docs/menu-system.md`: rotate = change selected item; press = advance
selection (title row → items → wrap); on the title row, rotate pages and press to
enter paging. Long press (>500 ms) toggles the **grain-field view** (§7).

---

## 6. SD card samples

### Card layout

```
0:/dirac/*.wav        16- or 24-bit PCM, mono or stereo, any rate
```

Scan `0:/dirac/` at boot with `f_opendir`/`f_readdir`, collect up to 64 filenames
into a fixed table (`libDaisy/src/util/FileTable.h` may serve; otherwise a plain
`char names[64][32]`). No recursion, no sorting beyond directory order.

### Loading

1. `f_open` → parse the WAV header ourselves (`libDaisy/src/util/wav_format.h`
   has the structs; `WavParser.h`/`WavPlayer.h` are streaming-oriented and not a
   good fit for a whole-file load).
2. Read in ~8 KB chunks into an **AXI SRAM staging buffer** (SDMMC DMA constraint,
   §2), convert to float, mono-sum, and append to the SDRAM store.
3. Resample: **don't**, at first. Store at the file's rate and let a
   `rateRatio = fileRate / 48000` multiply into the grain pitch increment. This is
   free (the grain read is already a variable-rate interpolated read) and handles
   44.1 k files correctly with no resampler.
4. Cap at the store size; truncate longer files and show `TRUNC` on the display.

### Concurrency — the part that will bite

`f_read` blocks for tens of milliseconds. The audio callback must never see a
half-written buffer or a `data`/`frames` pair that disagree.

- Loading runs in the **main loop**, never the callback.
- Sequence: set `engine.SetSample({})` (→ live mode, grains finish naturally and
  the ring keeps running) → wait one block → load → `SetSample(newSample)`.
- Publish the descriptor with a single atomic-ish store of a struct-index
  (double-buffer two `DiracSample` descriptors, flip an index), so the callback
  never reads a torn descriptor.
- Show a `LOAD…` progress line; audio continues (live mode) throughout.

### Menu

`SMPL` page: `File:` (rotate to select, press to load), `Detach` (→ live mode),
plus a read-only `len` seconds line.

---

## 7. Display

Four 6×8 rows on a 64-px-wide screen. Two views, toggled by long-press:

**Menu view** — title row (`*Page  .`) + 3 parameter rows, scrolling, per
`menu_system.{h,cpp}` from resonators/uzi (copy as-is, extend `MenuItemType` with
`Enum` for Window/Scale/assignment names and `Bipolar` for Tilt/Speed/HeadTune).

Pages (title, then items):

| Page | Items |
|---|---|
| `GRAIN` | Density, GrainLen, Grains, Playhead, Speed |
| `PITCH` | SemiShift, Psprd, Scale, Detune |
| `SHAPE` | Window, Texture, Tilt, TiltSpread |
| `SPACE` | Spread, Binaural, PosJtr, RevProb, Color |
| `FDBK` | Feedback, Mix, Level, Compress, Diffuse |
| `HEADS` | Playheads on/off, Heads, HeadSpread, HeadTune, Snap |
| `SMPL` | File, Detach, Freeze(capture), Hold |
| `MOD` | K1 dest, K2 dest, CV1 dest, CV1 depth, CV2 dest, CV2 depth |
| `MIDI` | Channel, CC map preset, Clock sync, Note mode |
| `IO` | IN-R mode, Out mode, V/Oct calibration |
| `SYS` | CPU load, free sample memory, version |

**Grain-field view** — the ER-301 signature display, scaled down. 64×24 px under a
1-row header. Each active grain is one pixel-scale glyph: X = pan (or read
position, toggled by pressing the encoder while in this view), Y = pitch,
brightness → the OLED is 1-bit, so encode envelope as *plotted/not plotted* with a
short phosphor decay in a `uint8_t[64][24]` age buffer rather than as brightness.
Reverse grains draw below the axis line. This is display-rate work only
(~30 Hz) — no audio-thread cost, exactly as in the original.

---

## 8. MIDI

`hw.midi.StartReceive()` in `Init`, `hw.midi.Listen()` + `HasEvents()`/`PopEvent()`
in the main loop (never the callback). Events are timestamped into the next
block's trigger buffer for sample-accurate spawns.

- **NoteOn** → set `voct` from `(note − 60)/12` octaves (summed with the CV V/Oct
  per the ER-301 clamp of ±4 oct), fire a trigger, velocity → `level` (or an
  assignable destination). This is what makes "playing sequences" work: a note
  sequence becomes a pitched grain sequence.
  - `Note mode` option: `retrig` (each note fires one grain), `gate` (note-on
    starts free-run density, note-off stops it), `latch` (note only sets pitch).
- **NoteOff** → per note mode.
- **CC** → the full parameter set, one CC per parameter, on a documented map
  (default: CC 16…47 in the page order above; CC 1 = Playhead, CC 7 = Level,
  CC 64 = Hold as a footswitch). A `CC map` menu item selects `default` /
  `learn` (next CC received binds to the currently selected menu item).
- **Program Change** → load sample by index from the SD file table. Cheap and very
  useful in a sequenced context.
- **Clock (0xF8) + Start/Stop** → optional `Clock sync`: derive the spawn hop from
  the MIDI clock at a selectable division (1/1…1/32T), overriding free-run
  density. This is the piece that makes granular clouds sit in a track.

MIDI values are 7-bit; smooth them with the same one-pole the knobs use so CC
sweeps don't zipper.

---

## 9. Build and flash size

`Makefile` mirrors `uzi/Makefile` plus:

```make
USE_FATFS = 1
OPT = -Os
```

**The flash budget is the main technical risk.** Internal flash is 128 KB;
`uzi` already needs `-Os`. Dirac adds FatFS (~20–25 KB), a WAV loader, the MIDI
layer, and a large menu table.

Mitigation ladder, in order:
1. `-Os`, `-ffast-math`, `--gc-sections` (the last two are already on).
2. Move constant tables to `const` (flash) where they are not built at runtime.
3. Drop `printf` float support / use fixed-point formatting in the display
   (`snprintf("%d")` only) — this alone is often several KB.
4. If it still doesn't fit: **switch to `APP_TYPE = BOOT_QSPI`** (8 MB QSPI, needs
   the Electrosmith bootloader flashed once, then `make program-dfu` targets QSPI).
   Note the `fatfs.h` caveat: with a bootloader app, verify the `FatFSInterface`
   object and the SD staging buffer still land in AXI SRAM. `BOOT_QSPI` (not
   `BOOT_SRAM`) is the safer of the two for this reason.

Decide this by measuring at the end of Phase 1, not by guessing now.

---

## 10. Host test harness

The ER-301 project ships `test/host/` and it is the reason its DSP is trustworthy.
This repo already has the pattern in `host_dsp/`. Extend it:

```
host_dsp/dirac_fixture.cpp   → links dirac_engine.cpp only (no libDaisy)
```

Port the ER-301 harness modes:

- `ident` — deterministic render to raw floats, for bit-exactness diffs against a
  reference before/after optimisation.
- `asan` — torture configs (1 ms grains at ±4 oct, 2 s grains, density 16, all
  features on) under `-fsanitize=address,undefined`.
- `seam` — click level at a sample loop boundary and at the live write-head
  collision, per channel (the v0.1.17 R-channel bug).
- `snapt` — mean |grain start − nearest onset| on a synthetic click track;
  expect the documented ~3.2× improvement at `snap 1`.

Add two bluemchen-specific ones:

- `cpu` — grains × block cost model, to size the grain cap for 480 MHz M7 before
  touching hardware.
- `wav` — feed the WAV loader a set of malformed/odd headers (24-bit, odd chunk
  sizes, `LIST` chunks before `data`, truncated files) and assert it refuses
  cleanly rather than loading garbage.

This is worth building *before* Phase 1 hardware testing, because the module has
no simulator and the OLED gives almost no debugging surface.

---

## 11. Phasing

Each phase ends with a working, flashable firmware.

**Phase 0 — scaffold.** `dirac/` folder, Makefile (`USE_FATFS=1`), `main.cpp`,
copied `menu_system`/`encoder_handler`/`display`, empty engine that passes audio
through. Verify `make -C dirac` and a flash. *Deliverable: it boots and shows a
menu.*

**Phase 1 — engine port, live mode only.** Port `Dirac.cpp`/`Dirac.h` to
`dirac_engine.{h,cpp}`: replace inlets with `DiracParams`, `FRAMELENGTH` with the
block argument, `od::Sample` with `DiracSample`, ring → SDRAM at 262144, envelope
table → 512. No sample mode, no MIDI. K1 = Playhead, K2 = Density, CV1 = V/Oct,
CV2 = Psprd. Three menu pages (`GRAIN`, `PITCH`, `SHAPE`). **Measure flash and CPU
here** and decide the `APP_TYPE` question. *Deliverable: a live-input granulator
that already sounds like Dirac.*

**Phase 2 — full parameter set + menus + mod matrix.** All 11 pages, assignable
knobs with pickup, assignable CV with bipolar depth, IN-R mode, output mode,
V/Oct calibration ported from `resonators`. *Deliverable: every ER-301 control
reachable.*

**Phase 3 — SD samples.** FatFS mount, directory scan, WAV parse, SDRAM store,
load-without-glitching handshake, `SMPL` page, sample-rate ratio, sample-mode
feedback/regeneration path (already in the ported engine). *Deliverable: granulate
files from the card.*

**Phase 4 — MIDI.** Notes → pitch + trigger, note modes, CC map + learn, program
change → sample select, clock sync. *Deliverable: sequenceable and
externally controllable.*

**Phase 5 — grain field display.** The 64×24 visualiser with the phosphor age
buffer and the pan/position axis toggle.

**Phase 6 — docs and polish.** `docs/dirac-manual.md` (following
`docs/resonators-manual.md`), CC map table, card layout, README entry, AGENTS.md
project note, and a `CLAUDE.md` fix for the 64×32 display claim.

---

## 11a. As built — where reality differed from the plan

The firmware in `dirac/` implements all six phases. Deviations worth recording:

- **Flash landed at 99.89%** (130 924 / 131 072 bytes) — it fits on internal
  flash, but only after four cuts beyond the planned `-Os`: snprintf replaced by
  `dirac_fmt.h` (~1.9 KB — newlib's nano-vfprintf drags in malloc and the FILE
  machinery), `powf` replaced by `expf(k·logf(x))` (~800 B), a 64-bit division
  in the load-progress calculation replaced by 32-bit (~880 B), and libm
  `tanhf` replaced by a Padé approximant in the feedback saturator (~800 B,
  since tanhf pulls in expm1f). The last is verified against libm in the
  harness: max error 1.05e-6, about −120 dB. `APP_TYPE = BOOT_QSPI` is left
  commented in the Makefile as the escape hatch.
- **Sample store is 8 M frames = 32 MB ≈ 2.9 minutes**, not the "16 MB ≈ 5.8
  minutes" of §4 — that line had the arithmetic wrong (16 MB of float32 is 87
  seconds, not 5.8 minutes). 32 MB gives the intended headroom.
- **The scan accumulator is float frames, not normalised double.** A float holds
  integers exactly to 2^24 (16.7 M frames, past the 8 M store) and the per-block
  increment is at most 192 frames, so precision is fine without the soft-float
  double routines.
- **`A_CAL` became two settings** (`cscl`/`cofs` on the `IO` page) rather than a
  capture action, matching the resonators calibration idiom, with the same
  1-second-debounced QSPI save.
- **A third modulation source** was added — the IN-R envelope follower has its
  own destination and depth (`MOD`/`modd`), rather than sharing a CV slot.
- **Snap measures 1.7–1.8×**, not the ER-301 README's 3.2×. Root-caused, not
  papered over: the coarse stage's 654-sample grid is wider than the ~240-sample
  window in which the energy-rise score is positive, so it finds an onset about
  a third of the time. The geometry is identical to the reference source, so
  this is the algorithm's behaviour on this material, not a port defect.
- **Trigger timing is block-quantised (~1 ms)**, not sample-accurate as §8
  claimed: libDaisy parses MIDI into a queue drained by the main loop, so the
  within-block offset is not recoverable. Musically inaudible for grain onsets.
- **Not yet tested on hardware.** Everything above is verified on the host
  harness (all modes pass, ASan/UBSan clean) and by the ARM build. Phase-1's
  "measure CPU on hardware" step is still outstanding — the `SYS → cpu` page
  exists for exactly that.

## 12. Decisions taken (flagging for review)

These are judgement calls made to keep the plan concrete; each is cheap to revisit:

1. **Mono sample store.** Halves memory and speeds the hot read; costs stereo-source
   granulation. The engine code path for stereo still exists if we want it back.
2. **Ring enlarged to 5.46 s** (from 2.73 s) — SDRAM is free.
3. **Envelope tables halved to 513 entries** to keep them in fast SRAM.
4. **No resampler** — sample-rate mismatch folds into the pitch increment.
5. **IN R is mode-switched** (capture / trigger / modulator) rather than fixed,
   since the module has no gate input and this is the cheapest way to get the
   ER-301's `trig` back.
6. **Grain cap stays at 16.** Raise only if the `cpu` harness says there is room;
   16 grains is the ER-301's own limit and the display can't show more usefully.
7. **Attribution.** Apache-2.0 requires shipping `LICENSE` and `NOTICE` with
   derivatives — copy both from `er301-Dirac` into `dirac/` and keep the SPDX
   headers on ported files. This repo's own LICENSE should stay as-is; add a note
   in `dirac/NOTICE` and the manual.
