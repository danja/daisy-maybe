# Project Name
TARGET = uzi

# Sources
CPP_SOURCES = main.cpp \
uzi_app.cpp \
uzi_dsp.cpp \
uzi_params.cpp \
uzi_ui.cpp \
uzi_spectral.cpp \
spectral_fft.cpp \
menu_system.cpp \
encoder_handler.cpp \
display.cpp \
$(BLUEMCHEN_DIR)/src/kxmx_bluemchen.cpp

# Reduce code size to fit flash
OPT = -Os

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

# Optional override: make -C uzi UZI_FFT_SIZE=2048
UZI_FFT_SIZE ?= 1024
CPP_DEFS += -DUZI_FFT_SIZE=$(UZI_FFT_SIZE)

# C++ standard
CPP_STANDARD = -std=gnu++17
