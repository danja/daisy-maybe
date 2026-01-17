TARGET = dsp_fixture

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SLIME_DIR = ../slime

SOURCES = dsp_fixture.cpp \
	$(SLIME_DIR)/spectral_processor.cpp \
	$(SLIME_DIR)/spectral_fft.cpp \
	$(SLIME_DIR)/spectral_processors.cpp

INCLUDES = -I$(SLIME_DIR)

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

clean:
	rm -f $(TARGET)
.PHONY: all clean
