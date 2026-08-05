TARGETS = dsp_fixture dirac_fixture

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SLIME_DIR = ../slime
DIRAC_DIR = ../dirac

SOURCES = dsp_fixture.cpp \
	$(SLIME_DIR)/spectral_processor.cpp \
	$(SLIME_DIR)/spectral_fft.cpp \
	$(SLIME_DIR)/spectral_processors.cpp

INCLUDES = -I$(SLIME_DIR)

DIRAC_SOURCES = dirac_fixture.cpp \
	$(DIRAC_DIR)/dirac_engine.cpp \
	$(DIRAC_DIR)/dirac_wav.cpp

DIRAC_INCLUDES = -I$(DIRAC_DIR)

all: $(TARGETS)

dsp_fixture: $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

dirac_fixture: $(DIRAC_SOURCES)
	$(CXX) $(CXXFLAGS) $(DIRAC_INCLUDES) $^ -o $@

# Sanitizer build of the Dirac harness (torture mode is the interesting one).
dirac_asan: $(DIRAC_SOURCES)
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
		$(DIRAC_INCLUDES) $^ -o $@

clean:
	rm -f $(TARGETS) dirac_asan
.PHONY: all clean
