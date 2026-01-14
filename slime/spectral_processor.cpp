#include "spectral_processor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kEps = 1.0e-9f;
}

void SpectralChannel::Init(float sampleRate, const float *window)
{
    (void)sampleRate;
    window_ = window;
    arm_rfft_fast_init_f32(&fft_, kFftSize);
    std::fill(&outputRing_[0], &outputRing_[kOutputBufferSize], 0.0f);
    std::fill(&smoothMag_[0], &smoothMag_[kNumBins], 0.0f);
    std::fill(&freezeMag_[0], &freezeMag_[kNumBins], 0.0f);
}

float SpectralChannel::ProcessSample(float input,
                                     SpectralProcess process,
                                     float timeRatio,
                                     float vibe)
{
    inputRing_[inputWrite_] = input;
    inputWrite_ = (inputWrite_ + 1) % kFftSize;

    float output = outputRing_[outputRead_];
    outputRing_[outputRead_] = 0.0f;
    outputRead_ = (outputRead_ + 1) % kOutputBufferSize;

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
        fftIn_[i] = window_[i] * inputRing_[source];
        source = (source + 1) % kFftSize;
    }

    arm_rfft_fast_f32(&fft_, fftIn_, fftOut_, 0);

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

    arm_rfft_fast_f32(&fft_, fftOut_, ifftOut_, 1);

    const float scale = 1.0f / static_cast<float>(kFftSize);
    size_t destination = outputWrite_;
    for (size_t i = 0; i < kFftSize; ++i)
    {
        const float sample = ifftOut_[i] * window_[i] * scale;
        outputRing_[destination] += sample;
        destination = (destination + 1) % kOutputBufferSize;
    }

    outputWrite_ = (outputWrite_ + kHopSize) % kOutputBufferSize;
}

void SpectralChannel::UnpackSpectrum()
{
    re_[0] = fftOut_[0];
    im_[0] = 0.0f;
    re_[kNumBins - 1] = fftOut_[1];
    im_[kNumBins - 1] = 0.0f;
    for (size_t k = 1; k < kNumBins - 1; ++k)
    {
        re_[k] = fftOut_[2 * k];
        im_[k] = fftOut_[2 * k + 1];
    }
}

void SpectralChannel::PackSpectrum()
{
    fftOut_[0] = re_[0];
    fftOut_[1] = re_[kNumBins - 1];
    for (size_t k = 1; k < kNumBins - 1; ++k)
    {
        fftOut_[2 * k] = re_[k];
        fftOut_[2 * k + 1] = im_[k];
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
