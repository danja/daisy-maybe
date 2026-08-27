# Project Name
TARGET = resonators

# Sources
CPP_SOURCES = main.cpp \
display.cpp \
encoder_handler.cpp \
menu_system.cpp \
res_midi.cpp \
res_sample.cpp \
res_wav.cpp \
$(BLUEMCHEN_DIR)/src/kxmx_bluemchen.cpp

# SD card sample loading needs FatFS linked in (adds ff.c, diskio.c and
# defines FILEIO_ENABLE_FATFS_READER).
USE_FATFS = 1

# This firmware now carries FatFS, a WAV loader, the MIDI layer and nine
# resonator models; -Os is not optional here.
OPT = -Os

# The build outgrew the Seed's 128 KB internal flash: it links there with 68
# bytes to spare, which is not headroom, so it targets the 8 MB QSPI region
# instead (1.6% used). That needs the Electrosmith bootloader flashed to the
# module ONCE with `make program-boot`, after which `make program-dfu` writes
# the firmware to QSPI exactly as before.
#
# BOOT_QSPI, not BOOT_SRAM: the FatFS objects and the SD staging buffer must
# stay in AXI SRAM for SDMMC1's DMA to reach them (see libDaisy/src/sys/fatfs.h).
#
# Comment this out to go back to internal flash — the link still fits, but only
# just, and the next string literal added anywhere will overflow it.
APP_TYPE = BOOT_QSPI

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
