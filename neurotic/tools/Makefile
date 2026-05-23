CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall

REPO_ROOT := ../..
DAISYSP_DIR ?= $(REPO_ROOT)/../DaisySP

TARGET := level_sanity
SOURCES := level_sanity.cpp \
	../algos/neurotic_algos.cpp \
	$(DAISYSP_DIR)/Source/Filters/svf.cpp

INCLUDES := -I.. \
	-I$(DAISYSP_DIR)/Source \
	-I$(DAISYSP_DIR)/Source/Utility

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
