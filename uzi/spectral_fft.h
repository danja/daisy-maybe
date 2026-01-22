#pragma once

#include <cstddef>
#include <cstdint>

#include "spectral_constants.h"

class SpectralFft
{
public:
    void Init();
    void Execute(float *re, float *im, bool inverse);

private:
    float cosTable_[kSpectralFftSize / 2]{};
    float sinTable_[kSpectralFftSize / 2]{};
    uint16_t bitRev_[kSpectralFftSize]{};
};
