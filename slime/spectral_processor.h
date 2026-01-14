#pragma once

#include <cstddef>

#include "arm_math.h"

enum class SpectralProcess
{
    Smear,
    Shift,
    Comb,
    Freeze,
    Count
};

class SpectralChannel
{
  public:
    void Init(float sampleRate, const float *window);
    float ProcessSample(float input,
                        SpectralProcess process,
                        float timeRatio,
                        float vibe);

  private:
    static constexpr size_t kFftSize = 1024;
    static constexpr size_t kHopSize = 256;
    static constexpr size_t kNumBins = kFftSize / 2 + 1;

    void ProcessFrame(SpectralProcess process, float timeRatio, float vibe);
    void UnpackSpectrum();
    void PackSpectrum();
    void ApplySmear(float vibe);
    void ApplyShift(float vibe);
    void ApplyComb(float vibe);
    void ApplyFreeze(float vibe);
    void ApplyTimeSmoothing(float timeRatio);

    float inputRing_[kFftSize]{};
    size_t inputWrite_ = 0;
    size_t hopCounter_ = 0;

    float fftIn_[kFftSize]{};
    float fftOut_[kFftSize]{};
    float ifftOut_[kFftSize]{};

    float re_[kNumBins]{};
    float im_[kNumBins]{};
    float mag_[kNumBins]{};
    float temp_[kNumBins]{};
    float tempIm_[kNumBins]{};
    float smoothMag_[kNumBins]{};
    float freezeMag_[kNumBins]{};

    static constexpr size_t kOutputBufferSize = 4096;
    float outputRing_[kOutputBufferSize]{};
    size_t outputRead_ = 0;
    size_t outputWrite_ = 0;

    const float *window_ = nullptr;
    arm_rfft_fast_instance_f32 fft_{};
};
