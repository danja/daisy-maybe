# Project Name
TARGET = resonators

# Sources
CPP_SOURCES = main.cpp \
display.cpp \
encoder_handler.cpp \
$(BLUEMCHEN_DIR)/src/kxmx_bluemchen.cpp

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
