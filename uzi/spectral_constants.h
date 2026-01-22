#pragma once

#include <cstddef>

#ifndef UZI_FFT_SIZE
#define UZI_FFT_SIZE 1024
#endif

constexpr size_t kSpectralFftSize = UZI_FFT_SIZE;
static_assert((kSpectralFftSize & (kSpectralFftSize - 1)) == 0, "FFT size must be a power of two.");
constexpr size_t kSpectralNumBins = kSpectralFftSize / 2 + 1;
