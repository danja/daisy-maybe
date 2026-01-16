# Project Name
TARGET = slime

# Sources
CPP_SOURCES = main.cpp \
display.cpp \
encoder_handler.cpp \
spectral_processor.cpp \
spectral_fft.cpp \
spectral_processors.cpp \
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

# C++ standard
CPP_STANDARD = -std=gnu++17

# Host-side DSP sanity check (Linux/macOS)
HOST_CXX ?= g++
HOST_CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
DSP_TEST_BIN = build/dsp_levels_test
DSP_TEST_SRC = tests/dsp_levels_test.cpp spectral_processor.cpp

.PHONY: dsp-levels-test

dsp-levels-test: $(DSP_TEST_BIN)
	./$(DSP_TEST_BIN)

$(DSP_TEST_BIN): $(DSP_TEST_SRC)
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(HOST_CXXFLAGS) -I. $^ -o $@
