# Quick Start Guide

Get your DSF oscillator running in 5 minutes!

## 1. Install Tools

### macOS
```bash
brew install gcc-arm-embedded dfu-util make
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install gcc-arm-none-eabi dfu-util build-essential
```

## 2. Clone Everything

```bash
# Create workspace
mkdir ~/bluemchen-workspace && cd ~/bluemchen-workspace

# Clone dependencies
git clone --recursive https://github.com/electro-smith/libDaisy.git
git clone --recursive https://github.com/electro-smith/DaisySP.git
git clone --recursive https://github.com/recursinging/kxmx_bluemchen.git

# Place your DSF project here
cd bluemchen-dsf
```

## 3. Build Libraries (One Time Only)

```bash
cd ~/bluemchen-workspace/libDaisy && make
cd ~/bluemchen-workspace/DaisySP && make
```

## 4. Build and Flash

```bash
cd ~/bluemchen-workspace/bluemchen-dsf

# Easy way (uses helper script)
./build.sh all

# Or manual way
make
# Put Daisy in DFU mode: hold BOOT, press RESET, release BOOT
make program-dfu
```

## 5. Use It!

- **Knob 1**: Frequency
- **Knob 2**: Harmonics
- **Encoder**: Change algorithm
- **CV 1**: Pitch control (V/Oct)
- **CV 2**: Alpha parameter

Done! 🎉

## Troubleshooting

**Build fails?**
- Check that all three repos (libDaisy, DaisySP, kxmx_bluemchen) are in parent directory
- Make sure you ran `make` in libDaisy and DaisySP first

**Can't flash?**
- Hold BOOT button, press RESET, then release BOOT
- Check USB cable supports data (not just power)
- Try `dfu-util -l` to see if device is detected

**No sound?**
- Check Eurorack power cable
- Rotate Knob 1 to change frequency
- Verify audio cable is in output jack

Need help? Check the full README.md
