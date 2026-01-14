#include "spectral_processor.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace
{
constexpr float kEps = 1.0e-9f;
}

void SpectralChannel::FftPlan::Init()
{
    for (size_t i = 0; i < kFftSize / 2; ++i)
    {
        const float phase = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(kFftSize);
        cosTable[i] = std::cos(phase);
        sinTable[i] = std::sin(phase);
    }

    const size_t bits = 10;
    for (size_t i = 0; i < kFftSize; ++i)
    {
        size_t x = i;
        size_t y = 0;
        for (size_t b = 0; b < bits; ++b)
        {
            y = (y << 1) | (x & 1u);
            x >>= 1;
        }
        bitRev[i] = static_cast<uint16_t>(y);
    }
}

void SpectralChannel::FftPlan::Execute(float *re, float *im, bool inverse)
{
    for (size_t i = 0; i < kFftSize; ++i)
    {
        const size_t j = bitRev[i];
        if (j > i)
        {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (size_t size = 2; size <= kFftSize; size <<= 1)
    {
        const size_t half = size >> 1;
        const size_t step = kFftSize / size;
        for (size_t start = 0; start < kFftSize; start += size)
        {
            for (size_t k = 0; k < half; ++k)
            {
                const size_t idx = k * step;
                const float cosVal = cosTable[idx];
                const float sinVal = inverse ? sinTable[idx] : -sinTable[idx];
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
        const float scale = 1.0f / static_cast<float>(kFftSize);
        for (size_t i = 0; i < kFftSize; ++i)
        {
            re[i] *= scale;
            im[i] *= scale;
        }
    }
}

void SpectralChannel::Init(float sampleRate, const float *window)
{
    (void)sampleRate;
    window_ = window;
    fft_.Init();
    std::fill(&outputRing_[0], &outputRing_[kOutputBufferSize], 0.0f);
    std::fill(&smoothMag_[0], &smoothMag_[kNumBins], 0.0f);
    std::fill(&freezeMag_[0], &freezeMag_[kNumBins], 0.0f);
    outputPrimed_ = false;
    outputRead_ = 0;
    outputWrite_ = 0;

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
                                     float vibe)
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
        ProcessFrame(process, timeRatio, vibe);
    }

    return output;
}

void SpectralChannel::ProcessFrame(SpectralProcess process, float timeRatio, float vibe)
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

    switch (process)
    {
    case SpectralProcess::Smear:
        ApplySmear(vibe);
        break;
    case SpectralProcess::Shift:
        ApplyShift(vibe);
        break;
    case SpectralProcess::Comb:
        ApplyComb(vibe);
        break;
    case SpectralProcess::Freeze:
        ApplyFreeze(vibe);
        break;
    default:
        break;
    }

    ApplyTimeSmoothing(timeRatio);
    PackSpectrum();

    fft_.Execute(fftRe_, fftIm_, true);

    const size_t frameStart = outputWrite_;
    size_t destination = frameStart;
    for (size_t i = 0; i < kFftSize; ++i)
    {
        const float norm = overlapInv_[(frameStart + i) % kHopSize];
        const float sample = fftRe_[i] * window_[i] * norm;
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

void SpectralChannel::ApplySmear(float vibe)
{
    const int radius = 1 + static_cast<int>(vibe * 12.0f);
    for (size_t k = 0; k < kNumBins; ++k)
    {
        mag_[k] = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]);
    }

    for (size_t k = 0; k < kNumBins; ++k)
    {
        const int start = std::max(0, static_cast<int>(k) - radius);
        const int end = std::min(static_cast<int>(kNumBins - 1), static_cast<int>(k) + radius);
        float sum = 0.0f;
        for (int i = start; i <= end; ++i)
        {
            sum += mag_[static_cast<size_t>(i)];
        }
        const float avg = sum / static_cast<float>(end - start + 1);
        const float scale = avg / (mag_[k] + kEps);
        re_[k] *= scale;
        im_[k] *= scale;
    }
}

void SpectralChannel::ApplyShift(float vibe)
{
    const float scale = 0.5f + vibe * 1.5f;
    std::fill(&temp_[0], &temp_[kNumBins], 0.0f);
    std::fill(&tempIm_[0], &tempIm_[kNumBins], 0.0f);

    for (size_t k = 0; k < kNumBins; ++k)
    {
        const float src = static_cast<float>(k) * scale;
        if (src >= static_cast<float>(kNumBins - 1))
            continue;
        const size_t i0 = static_cast<size_t>(src);
        const size_t i1 = std::min(i0 + 1, kNumBins - 1);
        const float frac = src - static_cast<float>(i0);
        const float re = re_[i0] + (re_[i1] - re_[i0]) * frac;
        const float im = im_[i0] + (im_[i1] - im_[i0]) * frac;
        temp_[k] = re;
        tempIm_[k] = im;
    }

    for (size_t k = 0; k < kNumBins; ++k)
    {
        re_[k] = temp_[k];
        im_[k] = tempIm_[k];
    }
}

void SpectralChannel::ApplyComb(float vibe)
{
    const int period = 4 + static_cast<int>(vibe * 60.0f);
    const int width = std::max(1, period / 3);
    for (size_t k = 0; k < kNumBins; ++k)
    {
        const int slot = static_cast<int>(k) % period;
        const float gain = slot < width ? 1.0f : 0.2f;
        re_[k] *= gain;
        im_[k] *= gain;
    }
}

void SpectralChannel::ApplyFreeze(float vibe)
{
    const float decay = 0.9f + vibe * 0.099f;
    for (size_t k = 0; k < kNumBins; ++k)
    {
        const float mag = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]);
        freezeMag_[k] = std::max(mag, freezeMag_[k] * decay);
        const float scale = freezeMag_[k] / (mag + kEps);
        re_[k] *= scale;
        im_[k] *= scale;
    }
}

void SpectralChannel::ApplyTimeSmoothing(float timeRatio)
{
    const float clamped = std::clamp(timeRatio, 0.25f, 16.0f);
    const float alpha = std::clamp(1.0f / clamped, 0.02f, 1.0f);
    for (size_t k = 0; k < kNumBins; ++k)
    {
        const float mag = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]);
        smoothMag_[k] += alpha * (mag - smoothMag_[k]);
        const float scale = smoothMag_[k] / (mag + kEps);
        re_[k] *= scale;
        im_[k] *= scale;
    }
}
