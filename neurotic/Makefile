# Project Name
TARGET = neurotic

# Sources
CPP_SOURCES = main.cpp \
neurotic_app.cpp \
neurotic_params.cpp \
neurotic_ui.cpp \
neurotic_dsp.cpp \
algos/neurotic_algos.cpp \
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

# C++ standard
CPP_STANDARD = -std=gnu++17
