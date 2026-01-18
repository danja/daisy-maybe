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

float ShortestPhaseDelta(float from, float to)
{
    float delta = to - from;
    while (delta > static_cast<float>(M_PI))
        delta -= kTwoPi;
    while (delta < -static_cast<float>(M_PI))
        delta += kTwoPi;
    return delta;
}

void ThruProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    (void)frame;
    (void)time;
    (void)vibe;
}

const char *ThruProcessor::Name() const
{
    return "Thru";
}

void SmearProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Spatial smear radius controlled by vibe - increased range for more extreme effect
    const int radius = 1 + static_cast<int>(vibe * 120.0f);

    // Temporal smoothing: alpha = framePeriod/time = 0.00533/time
    // At 10ms: alpha≈0.5 (very fast), At 5s: alpha≈0.001 (very slow trails)
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);

    // First pass: compute current magnitudes and update temporal smoothing
    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.mag[k] = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        // Temporal smoothing: accumulate magnitude over time
        frame.smoothMag[k] += alpha * (frame.mag[k] - frame.smoothMag[k]);
    }

    // Second pass: apply spatial smearing using temporally-smoothed magnitudes
    for (size_t k = 0; k < frame.bins; ++k)
    {
        if (frame.smoothMag[k] < kMinMag)
        {
            frame.re[k] = 0.0f;
            frame.im[k] = 0.0f;
            continue;
        }

        // Spatial averaging - weighted toward neighbors for more dramatic smear
        const int start = std::max(0, static_cast<int>(k) - radius);
        const int end = std::min(static_cast<int>(frame.bins - 1), static_cast<int>(k) + radius);
        float sum = 0.0f;
        for (int i = start; i <= end; ++i)
        {
            sum += frame.smoothMag[static_cast<size_t>(i)];
        }
        const float avg = sum / static_cast<float>(end - start + 1);

        // Blend strongly toward average for more extreme smearing (no kMaxScale limit)
        // Mix between current magnitude and neighborhood average
        const float blendAmount = 0.7f;  // 70% average, 30% original
        const float targetMag = frame.smoothMag[k] * (1.0f - blendAmount) + avg * blendAmount;
        const float scale = (frame.smoothMag[k] > kEps) ? (targetMag / frame.smoothMag[k]) : 1.0f;

        frame.re[k] *= scale;
        frame.im[k] *= scale;
    }
}

const char *SmearProcessor::Name() const
{
    return "Smear";
}

void ShiftProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Vibe controls shift amount (pitch shift ratio)
    const float scale = 0.1f + vibe * 3.0f;

    // Time controls glide/portamento smoothing
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);

    std::fill(&frame.temp[0], &frame.temp[frame.bins], 0.0f);
    std::fill(&frame.tempIm[0], &frame.tempIm[frame.bins], 0.0f);

    // First pass: compute magnitudes/phases and apply temporal smoothing
    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.mag[k] = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        frame.phase[k] = std::atan2(frame.im[k], frame.re[k]);
        frame.smoothMag[k] += alpha * (frame.mag[k] - frame.smoothMag[k]);
    }

    // Second pass: apply frequency shifting using smoothed magnitudes
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float src = static_cast<float>(k) * scale;
        if (src >= static_cast<float>(frame.bins - 1))
            continue;
        const size_t i0 = static_cast<size_t>(src);
        const size_t i1 = std::min(i0 + 1, frame.bins - 1);
        const float frac = src - static_cast<float>(i0);

        const float mag = frame.smoothMag[i0] + (frame.smoothMag[i1] - frame.smoothMag[i0]) * frac;
        const float phase = frame.phase[i0] + ShortestPhaseDelta(frame.phase[i0], frame.phase[i1]) * frac;
        frame.temp[k] = mag * std::cos(phase);
        frame.tempIm[k] = mag * std::sin(phase);
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

void CombProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Vibe controls comb period
    const int period = 3 + static_cast<int>(vibe * 80.0f);
    const int width = std::max(1, period / 4);

    // Time controls resonance/decay of combed bins
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);

    // First pass: compute magnitudes and apply temporal smoothing
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        frame.smoothMag[k] += alpha * (mag - frame.smoothMag[k]);
    }

    // Second pass: apply comb filter to smoothed magnitudes
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        const int slot = static_cast<int>(k) % period;
        const float gain = slot < width ? 1.0f : 0.05f;

        const float targetMag = frame.smoothMag[k] * gain;
        const float scale = (mag > kEps) ? (targetMag / mag) : 0.0f;
        frame.re[k] *= scale;
        frame.im[k] *= scale;
    }
}

const char *CombProcessor::Name() const
{
    return "Comb";
}

void FreezeProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Time controls freeze decay: longer time = slower decay = longer sustain
    // Decay coefficient: at 10ms→fast decay, at 5s→very slow decay
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);
    const float decay = 1.0f - alpha;

    // Vibe controls freeze threshold/sensitivity
    const float threshold = vibe * 0.5f;

    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);

        // Hold peaks with time-controlled decay
        if (mag > threshold)
        {
            frame.freezeMag[k] = std::max(mag, frame.freezeMag[k] * decay);
        }
        else
        {
            frame.freezeMag[k] *= decay;
        }

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

void GateProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Time controls release envelope speed
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);

    // First pass: compute magnitudes and apply envelope
    float maxMag = 0.0f;
    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.mag[k] = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        if (frame.mag[k] > maxMag)
            maxMag = frame.mag[k];

        // Envelope: fast attack, time-controlled release
        if (frame.mag[k] > frame.smoothMag[k])
        {
            frame.smoothMag[k] = frame.mag[k];
        }
        else
        {
            frame.smoothMag[k] += alpha * (frame.mag[k] - frame.smoothMag[k]);
        }
    }

    // Vibe controls threshold
    const float amount = 0.15f + std::clamp(vibe, 0.0f, 1.0f) * 0.85f;
    const float threshold = maxMag * amount;
    const float knee = std::max(threshold * 0.1f, kMinMag);

    // Second pass: apply gate using enveloped magnitudes
    for (size_t k = 0; k < frame.bins; ++k)
    {
        if (frame.smoothMag[k] < kMinMag)
        {
            frame.re[k] = 0.0f;
            frame.im[k] = 0.0f;
            continue;
        }

        float gain = 1.0f;
        if (frame.smoothMag[k] < threshold)
        {
            gain = frame.smoothMag[k] / (threshold + kEps);
        }
        if (frame.smoothMag[k] < knee)
        {
            gain *= frame.smoothMag[k] / (knee + kEps);
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

void TiltProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Vibe controls tilt amount
    const float tilt = ((std::clamp(vibe, 0.0f, 1.0f) * 2.0f) - 1.0f) * 3.0f;

    // Time controls tilt smoothing
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);

    // First pass: compute magnitudes and apply temporal smoothing
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        frame.smoothMag[k] += alpha * (mag - frame.smoothMag[k]);
    }

    // Second pass: apply tilt to smoothed magnitudes
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        const float pos = static_cast<float>(k) / static_cast<float>(frame.bins - 1);
        float gain = 1.0f + tilt * (pos - 0.5f) * 2.4f;
        gain = std::clamp(gain, 0.05f, 2.0f);

        const float targetMag = frame.smoothMag[k] * gain;
        const float scale = (mag > kEps) ? (targetMag / mag) : 0.0f;
        frame.re[k] *= scale;
        frame.im[k] *= scale;
    }
}

const char *TiltProcessor::Name() const
{
    return "Tilt";
}

void FoldProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Vibe controls fold center point
    const float center = std::clamp(vibe, 0.0f, 1.0f) * static_cast<float>(frame.bins - 1);

    // Time controls fold smoothing
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);

    std::fill(&frame.temp[0], &frame.temp[frame.bins], 0.0f);
    std::fill(&frame.tempIm[0], &frame.tempIm[frame.bins], 0.0f);

    // First pass: compute magnitudes/phases and apply temporal smoothing
    for (size_t k = 0; k < frame.bins; ++k)
    {
        frame.mag[k] = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        frame.phase[k] = std::atan2(frame.im[k], frame.re[k]);
        frame.smoothMag[k] += alpha * (frame.mag[k] - frame.smoothMag[k]);
    }

    // Second pass: apply folding using smoothed magnitudes
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float src = std::fabs(static_cast<float>(k) - center);
        const float mapped = center - src;
        const float clamped = std::clamp(mapped, 0.0f, static_cast<float>(frame.bins - 1));
        const size_t i0 = static_cast<size_t>(clamped);
        const size_t i1 = std::min(i0 + 1, frame.bins - 1);
        const float frac = clamped - static_cast<float>(i0);

        const float mag = frame.smoothMag[i0] + (frame.smoothMag[i1] - frame.smoothMag[i0]) * frac;
        const float phase = frame.phase[i0] + ShortestPhaseDelta(frame.phase[i0], frame.phase[i1]) * frac;
        frame.temp[k] = mag * std::cos(phase);
        frame.tempIm[k] = mag * std::sin(phase);
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

void PhaseProcessor::Process(SpectralFrame &frame, float time, float vibe) const
{
    // Vibe controls phase warp amount
    const float warp = (std::clamp(vibe, 0.0f, 1.0f) * 2.0f - 1.0f) * kTwoPi;

    // Time controls phase rotation rate smoothing
    const float alpha = std::clamp(0.00533f / time, 0.0005f, 0.95f);

    // First pass: compute magnitudes and apply temporal smoothing
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float mag = std::sqrt(frame.re[k] * frame.re[k] + frame.im[k] * frame.im[k]);
        frame.smoothMag[k] += alpha * (mag - frame.smoothMag[k]);
    }

    // Second pass: apply phase rotation with smoothed magnitudes
    for (size_t k = 0; k < frame.bins; ++k)
    {
        const float pos = static_cast<float>(k) / static_cast<float>(frame.bins - 1);
        const float angle = warp * pos;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float re = frame.re[k];
        const float im = frame.im[k];

        const float rotatedRe = re * c - im * s;
        const float rotatedIm = re * s + im * c;
        const float rotatedMag = std::sqrt(rotatedRe * rotatedRe + rotatedIm * rotatedIm);
        const float scale = (rotatedMag > kEps) ? (frame.smoothMag[k] / rotatedMag) : 0.0f;

        frame.re[k] = rotatedRe * scale;
        frame.im[k] = rotatedIm * scale;
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
