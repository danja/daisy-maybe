#include "spectral_fft.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void SpectralFft::Init()
{
    for (size_t i = 0; i < kSpectralFftSize / 2; ++i)
    {
        const float phase = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i)
                            / static_cast<float>(kSpectralFftSize);
        cosTable_[i] = std::cos(phase);
        sinTable_[i] = std::sin(phase);
    }

    const size_t bits = 10;
    for (size_t i = 0; i < kSpectralFftSize; ++i)
    {
        size_t x = i;
        size_t y = 0;
        for (size_t b = 0; b < bits; ++b)
        {
            y = (y << 1) | (x & 1u);
            x >>= 1;
        }
        bitRev_[i] = static_cast<uint16_t>(y);
    }
}

void SpectralFft::Execute(float *re, float *im, bool inverse)
{
    for (size_t i = 0; i < kSpectralFftSize; ++i)
    {
        const size_t j = bitRev_[i];
        if (j > i)
        {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (size_t size = 2; size <= kSpectralFftSize; size <<= 1)
    {
        const size_t half = size >> 1;
        const size_t step = kSpectralFftSize / size;
        for (size_t start = 0; start < kSpectralFftSize; start += size)
        {
            for (size_t k = 0; k < half; ++k)
            {
                const size_t idx = k * step;
                const float cosVal = cosTable_[idx];
                const float sinVal = inverse ? sinTable_[idx] : -sinTable_[idx];
                const size_t even = start + k;
                const size_t odd = even + half;
                const float tre = cosVal * re[odd] - sinVal * im[odd];
                const float tim = sinVal * re[odd] + cosVal * im[odd];
                const float ure = re[even];
                const float uim = im[even];
                re[even] = ure + tre;
                im[even] = uim + tim;
                re[odd] = ure - tre;
                im[odd] = uim - tim;
            }
        }
    }

    if (inverse)
    {
        const float scale = 1.0f / static_cast<float>(kSpectralFftSize);
        for (size_t i = 0; i < kSpectralFftSize; ++i)
        {
            re[i] *= scale;
            im[i] *= scale;
        }
    }
}
