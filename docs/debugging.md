# Debugging Guide

Complete guide to debugging the DSF Oscillator firmware using ST-Link and OpenOCD.

## Quick Start

1. **Connect Hardware**:
   - Connect ST-Link to Daisy Seed JTAG/SWD pins
   - Connect ST-Link to USB
   - Power the Daisy Seed (via Eurorack or USB)

2. **Launch Debugger**:
   - Press **F5** in VS Code
   - Or select Run → Start Debugging
   - Choose "Debug with ST-Link (OpenOCD)"

## Debug Configurations

### Debug with ST-Link (OpenOCD)
- Builds the project automatically
- Flashes to hardware
- Runs to `main()` and stops
- **Use this for normal debugging**

### Attach to ST-Link (OpenOCD)
- Attaches to already-running program
- Doesn't rebuild or reflash
- **Use this to debug live code**

## Troubleshooting

### "Could not connect to target"
- Check ST-Link USB connection (`lsusb | grep -i stm`)
- Verify Daisy Seed is powered
- Check JTAG/SWD connections (SWDIO, SWCLK, GND)

### "Error: libusb_open() failed with LIBUSB_ERROR_ACCESS"
Add udev rules for ST-Link:
```bash
sudo tee /etc/udev/rules.d/49-stlinkv2.rules > /dev/null <<'RULES'
# STLink V2
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0666"
# STLink V2-1
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="0666"
# STLink V3
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374e", MODE="0666"
RULES

sudo udevadm control --reload-rules
sudo udevadm trigger
```

Then reconnect your ST-Link.

### "Error in final launch sequence"
- Make sure project is built: `make`
- Check that `build/daisy-dsf.elf` exists
- Try cleaning and rebuilding: `make clean && make`

## Testing ST-Link Connection

Test OpenOCD connection manually:
```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg
```

You should see output like:
```
Info : clock speed 1800 kHz
Info : STLINK V2J27S7 (API v2) VID:PID 0483:3748
Info : Target voltage: 3.244659
Info : stm32h7x.cpu0: hardware has 8 breakpoints, 4 watchpoints
```

Press Ctrl+C to exit.

## Features

- **Breakpoints**: Click left of line numbers
- **Step Through**: F10 (over), F11 (into), Shift+F11 (out)
- **Variables**: Hover over variables or check Debug sidebar
- **Peripheral Registers**: Expand "Peripherals" in Debug sidebar (SVD file loaded)
- **Memory View**: Right-click variable → "View Binary Data"

## Build Tasks

Press **Ctrl+Shift+B** to access build tasks:
- **Build** (default)
- Clean
- Build and Flash
- Flash Only
