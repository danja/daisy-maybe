# Dependencies

Libraries and tools required to build the daisy-maybe projects.

## Libraries (required)

These repos must be cloned as siblings to `daisy-maybe/`:

- **libDaisy** (core Daisy Seed library)
- **DaisySP** (DSP utilities)
- **kxmx_bluemchen** (hardware abstraction for the module)

Recommended clone layout:
```
github/
├── libDaisy/
├── DaisySP/
├── kxmx_bluemchen/
└── daisy-maybe/
```

Build the libraries once:
```
cd libDaisy && make && cd ..
cd DaisySP && make && cd ..
```

## Toolchain (required)

- **ARM GCC**: `arm-none-eabi-gcc` / `arm-none-eabi-g++`
- **Make**

## Flashing Tools

Choose one:

- **dfu-util** for DFU flashing
- **ST‑Link** (`st-flash`, OpenOCD, or vendor tool)

## Optional

- **Python 3** (for helper scripts or tooling)
