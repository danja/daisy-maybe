#include "spectral_processors.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace
{
constexpr float kEps = 1.0e-9f;
constexpr float kMinMag = 1.0e-6f;
constexpr float kMaxScale = 3.0f;
constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);

ThruProcessor thruProcessor;
SmearProcessor smearProcessor;
ShiftProcessor shiftProcessor;
CombProcessor combProcessor;
FreezeProcessor freezeProcessor;
GateProcessor gateProcessor;
TiltProcessor tiltProcessor;
FoldProcessor foldProcessor;
PhaseProcessor phaseProcessor;
} // namespace

void ThruProcessor::Process(SpectralFrame &frame, float vibe) const
{
    (void)frame;
    (void)vibe;
}

const char *ThruProcessor::Name() const
{
    return "Thru";
}

void SmearProcessor::Process(SpectralFrame &frame, float vibe) const
{
    const int radius = 1 + static_cast<int>(vibe * 80.0f);
    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.mag[k] = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
    }

    for (size_t k = 0; k < frame.bins; ++k)
    {
        if (frame.mag[k] < kMinMag)
        {
            frame.re[k] = 0.0f;
            frame.im[k] = 0.0f;
            continue;
        }
        const int start = std::max(0, static_cast<int>(k) - radius);
        const int end = std::min(static_cast<int>(frame.bins - 1), static_cast<int>(k) + radius);
        float sum = 0.0f;
        for (int i = start; i <= end; ++i)
        {
            sum += frame.mag[static_cast<size_t>(i)];
        }
        const float avg = sum / static_cast<float>(end - start + 1);
        const float scale = std::min(avg / (frame.mag[k] + kEps), kMaxScale);
        frame.re[k] *= scale;
        frame.im[k] *= scale;
    }
}

const char *SmearProcessor::Name() const
{
    return "Smear";
}

void ShiftProcessor::Process(SpectralFrame &frame, float vibe) const
{
    const float scale = 0.1f + vibe * 3.0f;
    std::fill(&frame.temp[0], &frame.temp[frame.bins], 0.0f);
    std::fill(&frame.tempIm[0], &frame.tempIm[frame.bins], 0.0f);

    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float src = static_cast<float>(k) * scale;
        if (src >= static_cast<float>(frame.bins - 1))
            continue;
        const size_t i0 = static_cast<size_t>(src);
        const size_t i1 = std::min(i0 + 1, frame.bins - 1);
        const float frac = src - static_cast<float>(i0);
        const float re = frame.re[i0] + (frame.re[i1] - frame.re[i0]) * frac;
        const float im = frame.im[i0] + (frame.im[i1] - frame.im[i0]) * frac;
        frame.temp[k] = re;
        frame.tempIm[k] = im;
    }

    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.re[k] = frame.temp[k];
        frame.im[k] = frame.tempIm[k];
    }
}

const char *ShiftProcessor::Name() const
{
    return "Shift";
}

void CombProcessor::Process(SpectralFrame &frame, float vibe) const
{
    const int period = 3 + static_cast<int>(vibe * 80.0f);
    const int width = std::max(1, period / 4);
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const int slot = static_cast<int>(k) % period;
        const float gain = slot < width ? 1.0f : 0.05f;
        frame.re[k] *= gain;
        frame.im[k] *= gain;
    }
}

const char *CombProcessor::Name() const
{
    return "Comb";
}

void FreezeProcessor::Process(SpectralFrame &frame, float vibe) const
{
    const float decay = 0.9f + vibe * 0.099f;
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        frame.freezeMag[k] = std::max(mag, frame.freezeMag[k] * decay);
        if (mag < kMinMag)
        {
            if (frame.freezeMag[k] < kMinMag)
            {
                frame.re[k] = 0.0f;
                frame.im[k] = 0.0f;
            }
            continue;
        }
        const float scale = std::min(frame.freezeMag[k] / (mag + kEps), kMaxScale);
        frame.re[k] *= scale;
        frame.im[k] *= scale;
    }
}

const char *FreezeProcessor::Name() const
{
    return "Freeze";
}

void GateProcessor::Process(SpectralFrame &frame, float vibe) const
{
    float maxMag = 0.0f;
    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.mag[k] = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        if (frame.mag[k] > maxMag)
            maxMag = frame.mag[k];
    }
    const float amount = 0.15f + std::clamp(vibe, 0.0f, 1.0f) * 0.85f;
    const float threshold = maxMag * amount;
    const float knee = std::max(threshold * 0.1f, kMinMag);

    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = frame.mag[k];
        if (mag < kMinMag)
        {
            frame.re[k] = 0.0f;
            frame.im[k] = 0.0f;
            continue;
        }
        float gain = 1.0f;
        if (mag < threshold)
        {
            gain = mag / (threshold + kEps);
        }
        if (mag < knee)
        {
            gain *= mag / (knee + kEps);
        }
        gain = std::clamp(gain, 0.0f, 1.0f);
        frame.re[k] *= gain;
        frame.im[k] *= gain;
    }
}

const char *GateProcessor::Name() const
{
    return "Gate";
}

void TiltProcessor::Process(SpectralFrame &frame, float vibe) const
{
    const float tilt = ((std::clamp(vibe, 0.0f, 1.0f) * 2.0f) - 1.0f) * 3.0f;
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float pos = static_cast<float>(k) / static_cast<float>(frame.bins - 1);
        float gain = 1.0f + tilt * (pos - 0.5f) * 2.4f;
        gain = std::clamp(gain, 0.05f, 6.0f);
        frame.re[k] *= gain;
        frame.im[k] *= gain;
    }
}

const char *TiltProcessor::Name() const
{
    return "Tilt";
}

void FoldProcessor::Process(SpectralFrame &frame, float vibe) const
{
    const float center = std::clamp(vibe, 0.0f, 1.0f) * static_cast<float>(frame.bins - 1);
    std::fill(&frame.temp[0], &frame.temp[frame.bins], 0.0f);
    std::fill(&frame.tempIm[0], &frame.tempIm[frame.bins], 0.0f);

    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float src = std::fabs(static_cast<float>(k) - center);
        const float mapped = center - src;
        const float clamped = std::clamp(mapped, 0.0f, static_cast<float>(frame.bins - 1));
        const size_t i0 = static_cast<size_t>(clamped);
        const size_t i1 = std::min(i0 + 1, frame.bins - 1);
        const float frac = clamped - static_cast<float>(i0);
        frame.temp[k] = frame.re[i0] + (frame.re[i1] - frame.re[i0]) * frac;
        frame.tempIm[k] = frame.im[i0] + (frame.im[i1] - frame.im[i0]) * frac;
    }

    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.re[k] = frame.temp[k];
        frame.im[k] = frame.tempIm[k];
    }
}

const char *FoldProcessor::Name() const
{
    return "Fold";
}

void PhaseProcessor::Process(SpectralFrame &frame, float vibe) const
{
    const float warp = (std::clamp(vibe, 0.0f, 1.0f) * 2.0f - 1.0f) * kTwoPi;
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float pos = static_cast<float>(k) / static_cast<float>(frame.bins - 1);
        const float angle = warp * pos;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float re = frame.re[k];
        const float im = frame.im[k];
        frame.re[k] = re * c - im * s;
        frame.im[k] = re * s + im * c;
    }
}

const char *PhaseProcessor::Name() const
{
    return "Phase";
}

const SpectralProcessor &GetProcessor(int processIndex)
{
    switch (processIndex)
    {
    case 0:
        return thruProcessor;
    case 1:
        return smearProcessor;
    case 2:
        return shiftProcessor;
    case 3:
        return combProcessor;
    case 4:
        return freezeProcessor;
    case 5:
        return gateProcessor;
    case 6:
        return tiltProcessor;
    case 7:
        return foldProcessor;
    case 8:
        return phaseProcessor;
    default:
        return thruProcessor;
    }
}
