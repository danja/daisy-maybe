#pragma once

#include <cstddef>

constexpr size_t kSpectralFftSize = 1024;
constexpr size_t kSpectralHopSize = 256;
constexpr size_t kSpectralNumBins = kSpectralFftSize / 2 + 1;
