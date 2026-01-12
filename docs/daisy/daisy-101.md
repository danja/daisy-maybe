ensure you have:
    - The toolchain installed (see https://daisy.audio/tutorials/Understanding-the-Toolchain/)
    - Built the libDaisy and DaisySP libraries first by running:
    cd /home/danny/github/DaisyExamples
  ./ci/build_libs.sh

  cd seed/Blink/

  make

  Connect Daisy Seed via USB.
Put Daisy into bootloader mode by holding the BOOT button down, and then pressing the RESET button. Once you release the RESET button, you can also let go of the BOOT button.

 dfu-util -l

  # Using USB (after entering bootloader mode)
  make program-dfu
  
  # OR using JTAG/SWD adapter (like STLink)
    sudo apt-get update
  sudo apt-get install openocd

  sudo make program

