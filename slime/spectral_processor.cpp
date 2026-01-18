#include "spectral_processor.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace
{
constexpr float kEps = 1.0e-9f;
constexpr float kMinMag = 1.0e-6f;
constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
constexpr float kWetGain = 0.7f;
constexpr float kSpecMagLimit = 2.0f;  // Adjusted for 1/N forward FFT scaling
constexpr bool kEnableTimeSmoothing = false;
constexpr float kTimeSmoothMaxScale = 3.0f;
constexpr float kNormMinScale = 0.25f;
constexpr float kNormMaxScale = 4.0f;

void LimitSpectrum(float *re, float *im, size_t count)
{
    float maxMag = 0.0f;
    for (size_t k = 0; k < count; ++k)
    {
        const float mag = std::sqrt(re[k] * re[k] + im[k] * im[k]);
        maxMag = std::max(maxMag, mag);
    }
    if (maxMag <= kSpecMagLimit || maxMag < kEps)
        return;
    const float scale = kSpecMagLimit / maxMag;
    for (size_t k = 0; k < count; ++k)
    {
        re[k] *= scale;
        im[k] *= scale;
    }
}

float ComputeMagRms(const float *re, const float *im, size_t count)
{
    double sum = 0.0;
    for (size_t k = 0; k < count; ++k)
    {
        const float mag = std::sqrt(re[k] * re[k] + im[k] * im[k]);
        sum += static_cast<double>(mag) * static_cast<double>(mag);
    }
    if (count == 0)
        return 0.0f;
    return std::sqrt(static_cast<float>(sum / static_cast<double>(count)));
}

void NormalizeSpectrum(float *re, float *im, size_t count, float targetRms)
{
    const float current = ComputeMagRms(re, im, count);
    if (current < kEps || targetRms < kEps)
        return;
    const float scale = std::clamp(targetRms / current, kNormMinScale, kNormMaxScale);
    for (size_t k = 0; k < count; ++k)
    {
        re[k] *= scale;
        im[k] *= scale;
    }
}
} // namespace

void SpectralChannel::Init(float sampleRate, const float *window)
{
    (void)sampleRate;
    fft_.Init();
    std::fill(&outputRing_[0], &outputRing_[kOutputBufferSize], 0.0f);
    std::fill(&smoothMag_[0], &smoothMag_[kNumBins], 0.0f);
    std::fill(&freezeMag_[0], &freezeMag_[kNumBins], 0.0f);
    std::fill(&prevPhase_[0], &prevPhase_[kNumBins], 0.0f);
    std::fill(&sumPhase_[0], &sumPhase_[kNumBins], 0.0f);
    outputPrimed_ = false;
    outputRead_ = 0;
    outputWrite_ = 0;

    SetWindow(window);
}

void SpectralChannel::SetWindow(const float *window)
{
    window_ = window;
    if (!window_)
        return;
    const size_t overlap = kFftSize / kHopSize;
    for (size_t i = 0; i < kHopSize; ++i)
    {
        float sum = 0.0f;
        for (size_t m = 0; m < overlap; ++m)
        {
            const size_t idx = i + m * kHopSize;
            const float w = window_[idx];
            sum += w * w;
        }
        overlapInv_[i] = (sum > kEps) ? (1.0f / sum) : 1.0f;
    }
}

float SpectralChannel::ProcessSample(float input,
                                     SpectralProcess process,
                                     float timeRatio,
                                     float vibe,
                                     float preserve,
                                     float spectralGain,
                                     float ifftGain,
                                     float olaGain,
                                     bool  phaseContinuity,
                                     bool  normalizeSpectrum,
                                     bool  limitSpectrum)
{
    inputRing_[inputWrite_] = input;
    inputWrite_ = (inputWrite_ + 1) % kFftSize;

    float output = 0.0f;
    if (outputPrimed_)
    {
        output = outputRing_[outputRead_];
        outputRing_[outputRead_] = 0.0f;
        outputRead_ = (outputRead_ + 1) % kOutputBufferSize;
    }

    hopCounter_++;
    if (hopCounter_ >= kHopSize)
    {
        hopCounter_ = 0;
        ProcessFrame(process,
                     timeRatio,
                     vibe,
                     preserve,
                     spectralGain,
                     ifftGain,
                     olaGain,
                     phaseContinuity,
                     normalizeSpectrum,
                     limitSpectrum);
    }

    return output;
}

void SpectralChannel::ProcessFrame(SpectralProcess process,
                                   float timeRatio,
                                   float vibe,
                                   float preserve,
                                   float spectralGain,
                                   float ifftGain,
                                   float olaGain,
                                   bool  phaseContinuity,
                                   bool  normalizeSpectrum,
                                   bool  limitSpectrum)
{
    size_t source = inputWrite_;
    for (size_t i = 0; i < kFftSize; ++i)
    {
        fftRe_[i] = window_[i] * inputRing_[source];
        fftIm_[i] = 0.0f;
        source = (source + 1) % kFftSize;
    }

    fft_.Execute(fftRe_, fftIm_, false);

    UnpackSpectrum();
    const float preRms = ComputeMagRms(re_, im_, kNumBins);
    for (size_t k = 0; k < kNumBins; ++k)
    {
        const float re = re_[k];
        const float im = im_[k];
        mag_[k] = std::sqrt(re * re + im * im);
        phase_[k] = std::atan2(im, re);
        origRe_[k] = re_[k];
        origIm_[k] = im_[k];
    }
    SpectralFrame frame;
    frame.bins = kNumBins;
    frame.re = re_;
    frame.im = im_;
    frame.mag = mag_;
    frame.phase = phase_;
    frame.temp = temp_;
    frame.tempIm = tempIm_;
    frame.smoothMag = smoothMag_;
    frame.freezeMag = freezeMag_;

    GetProcessor(static_cast<int>(process)).Process(frame, timeRatio, vibe);

    // Phase continuity (phase vocoder) for time-stretched effects
    // Note: ApplyPhaseContinuity extracts mag/phase from re/im internally
    if (phaseContinuity && process != SpectralProcess::Thru)
    {
        ApplyPhaseContinuity();
    }
    if (kEnableTimeSmoothing && process != SpectralProcess::Thru)
    {
        ApplyTimeSmoothing(timeRatio);
    }
    if (process != SpectralProcess::Thru)
    {
        if (normalizeSpectrum)
        {
            NormalizeSpectrum(re_, im_, kNumBins, preRms);
        }
        if (preserve > 0.0f)
        {
            const float keep = std::clamp(preserve, 0.0f, 1.0f);
            const float mix = 1.0f - keep;
            for (size_t k = 0; k < kNumBins; ++k)
            {
                re_[k] = re_[k] * mix + origRe_[k] * keep;
                im_[k] = im_[k] * mix + origIm_[k] * keep;
            }
        }
    }
    if (spectralGain != 1.0f)
    {
        const float gain = std::clamp(spectralGain, 0.0f, 4.0f);
        for (size_t k = 0; k < kNumBins; ++k)
        {
            re_[k] *= gain;
            im_[k] *= gain;
        }
    }
    if (limitSpectrum)
    {
        LimitSpectrum(re_, im_, kNumBins);
    }
    PackSpectrum();

    fft_.Execute(fftRe_, fftIm_, true);
    if (ifftGain != 1.0f)
    {
        const float gain = std::clamp(ifftGain, 0.0f, 4.0f);
        for (size_t i = 0; i < kFftSize; ++i)
        {
            fftRe_[i] *= gain;
        }
    }

    const size_t frameStart = outputWrite_;
    size_t destination = frameStart;
    const float ola = std::clamp(olaGain, 0.0f, 4.0f);
    for (size_t i = 0; i < kFftSize; ++i)
    {
        const float norm = overlapInv_[(frameStart + i) % kHopSize];
        const float sample = fftRe_[i] * window_[i] * norm * kWetGain * ola;
        outputRing_[destination] += sample;
        destination = (destination + 1) % kOutputBufferSize;
    }

    outputWrite_ = (outputWrite_ + kHopSize) % kOutputBufferSize;
    if (!outputPrimed_)
    {
        outputRead_ = frameStart;
        outputPrimed_ = true;
    }
}

void SpectralChannel::UnpackSpectrum()
{
    re_[0] = fftRe_[0];
    im_[0] = 0.0f;
    re_[kNumBins - 1] = fftRe_[kFftSize / 2];
    im_[kNumBins - 1] = 0.0f;
    for (size_t k = 1; k < kNumBins - 1; ++k)
    {
        re_[k] = fftRe_[k];
        im_[k] = fftIm_[k];
    }
}

void SpectralChannel::PackSpectrum()
{
    fftRe_[0] = re_[0];
    fftIm_[0] = 0.0f;
    fftRe_[kFftSize / 2] = re_[kNumBins - 1];
    fftIm_[kFftSize / 2] = 0.0f;

    for (size_t k = 1; k < kNumBins - 1; ++k)
    {
        fftRe_[k] = re_[k];
        fftIm_[k] = im_[k];
        const size_t mirror = kFftSize - k;
        fftRe_[mirror] = re_[k];
        fftIm_[mirror] = -im_[k];
    }
}

void SpectralChannel::ApplyPhaseContinuity()
{
    const float phaseAdvance = kTwoPi * static_cast<float>(kHopSize) / static_cast<float>(kFftSize);
    for (size_t k = 1; k < kNumBins - 1; ++k)
    {
        const float mag = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]);
        if (mag < kMinMag)
        {
            re_[k] = 0.0f;
            im_[k] = 0.0f;
            continue;
        }

        const float phase = std::atan2(im_[k], re_[k]);
        float delta = phase - prevPhase_[k] - phaseAdvance * static_cast<float>(k);
        while (delta > static_cast<float>(M_PI))
            delta -= kTwoPi;
        while (delta < -static_cast<float>(M_PI))
            delta += kTwoPi;

        sumPhase_[k] += phaseAdvance * static_cast<float>(k) + delta;
        prevPhase_[k] = phase;

        re_[k] = mag * std::cos(sumPhase_[k]);
        im_[k] = mag * std::sin(sumPhase_[k]);
    }
    re_[0] = re_[0];
    im_[0] = 0.0f;
    re_[kNumBins - 1] = re_[kNumBins - 1];
    im_[kNumBins - 1] = 0.0f;
}

void SpectralChannel::ApplyTimeSmoothing(float timeRatio)
{
    const float clamped = std::clamp(timeRatio, 0.01f, 5.0f);
    const float alpha = std::clamp(0.00533f / clamped, 0.0005f, 0.95f);
    for (size_t k = 0; k < kNumBins; ++k)
    {
        const float mag = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]);
        if (mag < kMinMag)
        {
            smoothMag_[k] *= 0.95f;
            if (smoothMag_[k] < kMinMag)
            {
                re_[k] = 0.0f;
                im_[k] = 0.0f;
            }
            continue;
        }
        smoothMag_[k] += alpha * (mag - smoothMag_[k]);
        const float scale = std::min(smoothMag_[k] / (mag + kEps), kTimeSmoothMaxScale);
        re_[k] *= scale;
        im_[k] *= scale;
    }
}
