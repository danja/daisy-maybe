# AGENTS.md

Guidance for working on the daisy-maybe repository (kxmx_bluemchen firmwares).

## Repo Layout

- `daisy-dsf/` : DSF synthesizer firmware.
- `resonators/` : Dual resonator delay firmware.
- `slime/` : FFT spectral processor firmware.
- `dirac/` : Granular synthesizer firmware (SD card samples, MIDI, mod matrix).
- `docs/` : User manuals and requirements.

## Dependencies

See `docs/dependencies.md` for required libraries and tools. The three external repos must be cloned as siblings to `daisy-maybe/`:

```
github/
├── libDaisy/
├── DaisySP/
├── kxmx_bluemchen/
└── daisy-maybe/
```

## Build and Flash

Build from repo root:
```
make -C daisy-dsf
make -C resonators
make -C slime
make -C dirac
```

Flash (DFU example):
```
make -C daisy-dsf program-dfu
make -C resonators program-dfu
make -C slime program-dfu
make -C dirac program-dfu
```

## Project Notes

- **daisy-dsf**: Uses DSF algorithms and formant synth. See `docs/dsf-manual.md`.
- **resonators**: Dual delay lines, calibration mode, bipolar knob/CV offset mapping. See `docs/resonators-manual.md`.
- **slime**: FFT spectral effects, debug pages, bipolar knob/CV mapping. See `docs/slime-manual.md`.
- **dirac**: Time-domain granular synth ported from the ER-301 (Apache-2.0 — keep
  `dirac/LICENSE` and `dirac/NOTICE` with any distribution). The DSP in
  `dirac_engine.{h,cpp}` is deliberately libDaisy-free so it builds on the host:
  test it with `make -C host_dsp dirac_fixture && ./host_dsp/dirac_fixture all`
  before flashing. Note it sits at ~99.9% of the 128 KB internal flash — adding
  code means either trimming or switching to `APP_TYPE = BOOT_QSPI`. See
  `docs/dirac-manual.md` and `docs/dirac-plan.md`.

## Adding a New Firmware Project

1. Create a new folder at repo root (e.g., `new-firmware/`).
2. Add a `Makefile` that includes `$(LIBDAISY_DIR)/core/Makefile` and references `kxmx_bluemchen/src/kxmx_bluemchen.cpp`.
3. Add `main.cpp` and any support files (display, encoder, dsp helpers).
4. Keep audio callbacks allocation-free and light; do control scanning in the main loop unless you need sample-accurate control.
5. Use OLED updates at ~30 Hz to avoid UI jitter.
6. Update `docs/` with a user manual and mention the project in `README.md`.

## Common Patterns

- Use `hw.ProcessAnalogControls()` and `hw.ProcessDigitalControls()` once per main loop unless you need audio-rate control.
- Encoder: use `Increment()` for rotations; `RisingEdge()`/`FallingEdge()` for presses.
- ADC raw debug: `hw.controls[CTRL_*].GetRawValue()` (12-bit ADC mapped into 16-bit range).

