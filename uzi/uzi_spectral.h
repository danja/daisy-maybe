#pragma once

#include <cstddef>
#include <cstdint>

#include "spectral_constants.h"
#include "spectral_fft.h"
#include "uzi_state.h"

class UziSpectralStereo
{
public:
    void Init(float sampleRate);
    void Reset();
    void SetHopSize(size_t hopSize);

    void ProcessSample(float inL,
                       float inR,
                       const UziRuntime &runtime,
                       float lfoValue,
                       size_t hopSize,
                       float &outL,
                       float &outR);

private:
    void BuildHannWindow();
    void ProcessFrame(const UziRuntime &runtime, float lfoValue);
    void UnpackSpectrum(int ch);
    void PackSpectrum(int ch);

    float sampleRate_ = 48000.0f;
    size_t hopSize_ = 256;
    size_t hopCounter_ = 0;
    size_t inputWrite_ = 0;

    static constexpr size_t kFftSize = kSpectralFftSize;
    static constexpr size_t kNumBins = kSpectralNumBins;
    static constexpr size_t kOutputBufferSize = 4096;

    float window_[kFftSize]{};
    float overlapInv_[kFftSize]{};

    float inputRing_[2][kFftSize]{};
    float fftRe_[2][kFftSize]{};
    float fftIm_[2][kFftSize]{};

    float re_[2][kNumBins]{};
    float im_[2][kNumBins]{};
    float origRe_[2][kNumBins]{};
    float origIm_[2][kNumBins]{};

    float outputRing_[2][kOutputBufferSize]{};
    size_t outputRead_ = 0;
    size_t outputWrite_ = 0;
    bool outputPrimed_ = false;

    size_t cutoffBin_ = 0;

    SpectralFft fft_{};
};
