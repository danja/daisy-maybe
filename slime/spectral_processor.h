#pragma once

#include <cstddef>


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

    struct FftPlan
    {
        void Init();
        void Execute(float *re, float *im, bool inverse);

        float    cosTable[kFftSize / 2]{};
        float    sinTable[kFftSize / 2]{};
        uint16_t bitRev[kFftSize]{};
    };

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

    float fftRe_[kFftSize]{};
    float fftIm_[kFftSize]{};

    float re_[kNumBins]{};
    float im_[kNumBins]{};
    float mag_[kNumBins]{};
    float temp_[kNumBins]{};
    float tempIm_[kNumBins]{};
    float smoothMag_[kNumBins]{};
    float freezeMag_[kNumBins]{};
    float overlapInv_[kHopSize]{};

    static constexpr size_t kOutputBufferSize = 4096;
    float outputRing_[kOutputBufferSize]{};
    size_t outputRead_ = 0;
    size_t outputWrite_ = 0;
    bool outputPrimed_ = false;

    const float *window_ = nullptr;
    FftPlan fft_{};
};
