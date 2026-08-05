# Project Name
TARGET = dirac

# Sources
CPP_SOURCES = main.cpp \
dirac_app.cpp \
dirac_engine.cpp \
dirac_params.cpp \
dirac_sample.cpp \
dirac_midi.cpp \
dirac_ui.cpp \
dirac_wav.cpp \
$(BLUEMCHEN_DIR)/src/kxmx_bluemchen.cpp

# SD card sample loading needs FatFS linked in (adds ff.c, diskio.c and
# defines FILEIO_ENABLE_FATFS_READER).
USE_FATFS = 1

# Internal flash is 128 KB and this firmware carries FatFS, a WAV loader and
# the MIDI layer; -Os is not optional here.
OPT = -Os

# HEADROOM WARNING: this build sits at ~99.9% of the 128 KB internal flash.
# Getting it to fit meant dropping snprintf (see dirac_fmt.h), powf, 64-bit
# division and libm's tanhf (see FastTanh in dirac_engine.h) — every remaining
# byte is spoken for. If you add code and the link overflows, uncomment the
# line below for the 8 MB QSPI region instead of trimming further. That needs
# the Electrosmith bootloader flashed once, after which `make program-dfu`
# targets QSPI. BOOT_QSPI (not BOOT_SRAM) is the one to use: the FatFS objects
# and the SD staging buffer must stay in AXI SRAM for SDMMC1's DMA to reach
# them (see libDaisy/src/sys/fatfs.h).
# APP_TYPE = BOOT_QSPI

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP
BLUEMCHEN_DIR = ../../kxmx_bluemchen

# Core location, and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Include paths
C_INCLUDES += \
-I$(BLUEMCHEN_DIR)/src \
-I.

# C++ standard
CPP_STANDARD = -std=gnu++17
