TARGETS = dsp_fixture dirac_fixture bodies_fixture loop_fixture

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SLIME_DIR = ../slime
DIRAC_DIR = ../dirac
RESONATORS_DIR = ../resonators

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

# modal_bodies.h is header-only and libDaisy-free, so the struck bodies build
# natively with no stubs.
bodies_fixture: bodies_fixture.cpp $(RESONATORS_DIR)/modal_bodies.h
	$(CXX) $(CXXFLAGS) -I$(RESONATORS_DIR) bodies_fixture.cpp -o $@

# The full model set needs DaisySP's Svf; it is portable C++, so one .cpp is
# the whole dependency.
DAISYSP_SRC = ../../DaisySP/Source
loop_fixture: loop_fixture.cpp $(RESONATORS_DIR)/resonator_models.h
	$(CXX) $(CXXFLAGS) -I$(RESONATORS_DIR) -I$(DAISYSP_SRC) \
		-I$(DAISYSP_SRC)/Filters -I$(DAISYSP_SRC)/Utility \
		loop_fixture.cpp $(DAISYSP_SRC)/Filters/svf.cpp -o $@

# Sanitizer build of the Dirac harness (torture mode is the interesting one).
dirac_asan: $(DIRAC_SOURCES)
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
		$(DIRAC_INCLUDES) $^ -o $@

clean:
	rm -f $(TARGETS) dirac_asan
.PHONY: all clean
