#include "uzi_spectral.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace
{
constexpr float kEps = 1.0e-9f;
constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
constexpr float kWetGain = 0.8f;
constexpr float kNotchDepth = 0.98f;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

size_t ClampHopSize(size_t hop)
{
    if (hop == 128 || hop == 256 || hop == 512 || hop == 1024)
    {
        return hop;
    }
    if (hop < 192)
    {
        return 128;
    }
    if (hop < 384)
    {
        return 256;
    }
    if (hop < 768)
    {
        return 512;
    }
    return 1024;
}

float ShortestPhaseDelta(float from, float to)
{
    float delta = to - from;
    while (delta > static_cast<float>(M_PI))
        delta -= kTwoPi;
    while (delta < -static_cast<float>(M_PI))
        delta += kTwoPi;
    return delta;
}
}

void UziSpectralStereo::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    fft_.Init();
    BuildHannWindow();

    cutoffBin_ = static_cast<size_t>((100.0f * static_cast<float>(kFftSize)) / sampleRate_ + 0.5f);
    cutoffBin_ = std::min(cutoffBin_, kNumBins - 1);

    SetHopSize(hopSize_);
    Reset();
}

void UziSpectralStereo::Reset()
{
    std::fill(&inputRing_[0][0], &inputRing_[0][kFftSize], 0.0f);
    std::fill(&inputRing_[1][0], &inputRing_[1][kFftSize], 0.0f);
    std::fill(&outputRing_[0][0], &outputRing_[0][kOutputBufferSize], 0.0f);
    std::fill(&outputRing_[1][0], &outputRing_[1][kOutputBufferSize], 0.0f);
    inputWrite_ = 0;
    hopCounter_ = 0;
    outputRead_ = 0;
    outputWrite_ = 0;
    outputPrimed_ = false;
}

void UziSpectralStereo::SetHopSize(size_t hopSize)
{
    hopSize_ = ClampHopSize(hopSize);
    const size_t overlap = kFftSize / hopSize_;
    for (size_t i = 0; i < hopSize_; ++i)
    {
        float sum = 0.0f;
        for (size_t m = 0; m < overlap; ++m)
        {
            const size_t idx = i + m * hopSize_;
            const float w = window_[idx];
            sum += w * w;
        }
        overlapInv_[i] = (sum > kEps) ? (1.0f / sum) : 1.0f;
    }
    for (size_t i = hopSize_; i < kFftSize; ++i)
    {
        overlapInv_[i] = overlapInv_[i % hopSize_];
    }

    Reset();
}

void UziSpectralStereo::ProcessSample(float inL,
                                      float inR,
                                      const UziRuntime &runtime,
                                      float lfoValue,
                                      size_t hopSize,
                                      float &outL,
                                      float &outR)
{
    if (hopSize != hopSize_)
    {
        SetHopSize(hopSize);
    }

    inputRing_[0][inputWrite_] = inL;
    inputRing_[1][inputWrite_] = inR;
    inputWrite_ = (inputWrite_ + 1) % kFftSize;

    outL = 0.0f;
    outR = 0.0f;
    if (outputPrimed_)
    {
        outL = outputRing_[0][outputRead_];
        outR = outputRing_[1][outputRead_];
        outputRing_[0][outputRead_] = 0.0f;
        outputRing_[1][outputRead_] = 0.0f;
        outputRead_ = (outputRead_ + 1) % kOutputBufferSize;
    }

    hopCounter_++;
    if (hopCounter_ >= hopSize_)
    {
        hopCounter_ = 0;
        ProcessFrame(runtime, lfoValue);
    }
}

void UziSpectralStereo::BuildHannWindow()
{
    for (size_t i = 0; i < kFftSize; ++i)
    {
        const float phase = static_cast<float>(i) / static_cast<float>(kFftSize);
        window_[i] = 0.5f - 0.5f * std::cos(kTwoPi * phase);
    }
}

void UziSpectralStereo::ProcessFrame(const UziRuntime &runtime, float lfoValue)
{
    size_t source = inputWrite_;
    const size_t frameStart = outputWrite_;
    for (int ch = 0; ch < 2; ++ch)
    {
        size_t idx = source;
        for (size_t i = 0; i < kFftSize; ++i)
        {
            fftRe_[ch][i] = window_[i] * inputRing_[ch][idx];
            fftIm_[ch][i] = 0.0f;
            idx = (idx + 1) % kFftSize;
        }
        fft_.Execute(fftRe_[ch], fftIm_[ch], false);
        UnpackSpectrum(ch);
        for (size_t k = 0; k < kNumBins; ++k)
        {
            origRe_[ch][k] = re_[ch][k];
            origIm_[ch][k] = im_[ch][k];
        }
    }

    const float spacing = std::max(1.0f, runtime.notchDistance * 240.0f);
    const float phaseShift = (runtime.phaseOffset + lfoValue * runtime.lfoDepth * 4.0f) * spacing;
    const int roundBins = 1 + static_cast<int>(runtime.binRounding * 24.0f);
    const float sigma = std::clamp(0.3f + runtime.blur * (spacing * 0.7f), 0.3f, spacing);

    const float cutoffHz = std::clamp(runtime.cutoffHz, 0.0f, 300.0f);
    size_t cutoffBin = static_cast<size_t>((cutoffHz * static_cast<float>(kFftSize)) / sampleRate_ + 0.5f);
    cutoffBin = std::min(cutoffBin, kNumBins - 1);

    for (size_t k = 0; k < kNumBins; ++k)
    {
        if (k <= cutoffBin)
        {
            re_[0][k] = origRe_[0][k];
            im_[0][k] = origIm_[0][k];
            re_[1][k] = origRe_[1][k];
            im_[1][k] = origIm_[1][k];
            continue;
        }

        const float binForPattern = std::round(static_cast<float>(k) / static_cast<float>(roundBins))
                                    * static_cast<float>(roundBins);
        const float binPhase = binForPattern + phaseShift;
        const float notchIndex = std::round(binPhase / spacing);
        const float center = notchIndex * spacing;
        const float dist = std::fabs(binPhase - center);
        const float notchShape = std::exp(-(dist * dist) / (2.0f * sigma * sigma));
        const float scale = 1.0f - kNotchDepth * notchShape;

        for (int ch = 0; ch < 2; ++ch)
        {
            re_[ch][k] *= scale;
            im_[ch][k] *= scale;
        }
    }

    const float xmix = Clamp01(runtime.xmix);
    if (xmix > 0.0f)
    {
        for (size_t k = cutoffBin + 1; k < kNumBins; ++k)
        {
            const float reL = re_[0][k];
            const float imL = im_[0][k];
            const float reR = re_[1][k];
            const float imR = im_[1][k];

            const float magL = std::sqrt(reL * reL + imL * imL);
            const float magR = std::sqrt(reR * reR + imR * imR);
            const float phaseL = std::atan2(imL, reL);
            const float phaseR = std::atan2(imR, reR);

            const float magLNew = magL * (1.0f - xmix) + magR * xmix;
            const float magRNew = magR * (1.0f - xmix) + magL * xmix;
            const float phaseLNew = phaseL + ShortestPhaseDelta(phaseL, phaseR) * xmix;
            const float phaseRNew = phaseR + ShortestPhaseDelta(phaseR, phaseL) * xmix;

            re_[0][k] = magLNew * std::cos(phaseLNew);
            im_[0][k] = magLNew * std::sin(phaseLNew);
            re_[1][k] = magRNew * std::cos(phaseRNew);
            im_[1][k] = magRNew * std::sin(phaseRNew);
        }
    }

    const float crossover = Clamp01(runtime.crossover);
    if (crossover > 0.0f)
    {
        for (size_t k = cutoffBin + 1; k < kNumBins; ++k)
        {
            const float reL = re_[0][k];
            const float imL = im_[0][k];
            const float reR = re_[1][k];
            const float imR = im_[1][k];
            re_[0][k] = reL * (1.0f - crossover) + reR * crossover;
            im_[0][k] = imL * (1.0f - crossover) + imR * crossover;
            re_[1][k] = reR * (1.0f - crossover) + reL * crossover;
            im_[1][k] = imR * (1.0f - crossover) + imL * crossover;
        }
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        PackSpectrum(ch);
        fft_.Execute(fftRe_[ch], fftIm_[ch], true);

        size_t destination = frameStart;
        for (size_t i = 0; i < kFftSize; ++i)
        {
            const float norm = overlapInv_[(frameStart + i) % hopSize_];
            const float sample = fftRe_[ch][i] * window_[i] * norm * kWetGain;
            outputRing_[ch][destination] += sample;
            destination = (destination + 1) % kOutputBufferSize;
        }
    }

    outputWrite_ = (outputWrite_ + hopSize_) % kOutputBufferSize;
    if (!outputPrimed_)
    {
        outputRead_ = frameStart;
        outputPrimed_ = true;
    }
}

void UziSpectralStereo::UnpackSpectrum(int ch)
{
    re_[ch][0] = fftRe_[ch][0];
    im_[ch][0] = 0.0f;
    re_[ch][kNumBins - 1] = fftRe_[ch][kFftSize / 2];
    im_[ch][kNumBins - 1] = 0.0f;
    for (size_t k = 1; k < kNumBins - 1; ++k)
    {
        re_[ch][k] = fftRe_[ch][k];
        im_[ch][k] = fftIm_[ch][k];
    }
}

void UziSpectralStereo::PackSpectrum(int ch)
{
    fftRe_[ch][0] = re_[ch][0];
    fftIm_[ch][0] = 0.0f;
    fftRe_[ch][kFftSize / 2] = re_[ch][kNumBins - 1];
    fftIm_[ch][kFftSize / 2] = 0.0f;

    for (size_t k = 1; k < kNumBins - 1; ++k)
    {
        fftRe_[ch][k] = re_[ch][k];
        fftIm_[ch][k] = im_[ch][k];
        const size_t mirror = kFftSize - k;
        fftRe_[ch][mirror] = re_[ch][k];
        fftIm_[ch][mirror] = -im_[ch][k];
    }
}
