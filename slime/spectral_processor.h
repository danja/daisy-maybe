#pragma once

#include <cstddef>

#include "spectral_constants.h"
#include "spectral_fft.h"
#include "spectral_processors.h"

enum class SpectralProcess
{
    Thru,
    Smear,
    Shift,
    Comb,
    Freeze,
    Gate,
    Tilt,
    Fold,
    Phase,
    Count
};

class SpectralChannel
{
  public:
    static constexpr size_t kFftSize = kSpectralFftSize;
    static constexpr size_t kHopSize = kSpectralHopSize;
    static constexpr size_t kNumBins = kSpectralNumBins;

    void Init(float sampleRate, const float *window);
    float ProcessSample(float input,
                        SpectralProcess process,
                        float timeRatio,
                        float vibe);

  private:
    void ProcessFrame(SpectralProcess process, float timeRatio, float vibe);
    void UnpackSpectrum();
    void PackSpectrum();
    void ApplyPhaseContinuity();
    void ApplyTimeSmoothing(float timeRatio);

    float inputRing_[kFftSize]{};
    size_t inputWrite_ = 0;
    size_t hopCounter_ = 0;

    float fftRe_[kFftSize]{};
    float fftIm_[kFftSize]{};

    float re_[kNumBins]{};
    float im_[kNumBins]{};
    float mag_[kNumBins]{};
    float temp_[kNumBins]{};
    float tempIm_[kNumBins]{};
    float smoothMag_[kNumBins]{};
    float freezeMag_[kNumBins]{};
    float prevPhase_[kNumBins]{};
    float sumPhase_[kNumBins]{};
    float overlapInv_[kHopSize]{};

    static constexpr size_t kOutputBufferSize = 4096;
    float outputRing_[kOutputBufferSize]{};
    size_t outputRead_ = 0;
    size_t outputWrite_ = 0;
    bool outputPrimed_ = false;

    const float *window_ = nullptr;
    SpectralFft fft_{};
};
