#pragma once

#include <algorithm>
#include <cmath>

struct DistortionSettings
{
    float depth = 0.0f;
    int folds = 1;
    float overdrive = 0.0f;
};

inline float ApplyWavefolder(float sample, float depth, int folds)
{
    if (depth <= 0.0f || folds <= 0)
    {
        return sample;
    }

    const float drive = 1.0f + depth * static_cast<float>(folds) * 2.0f;
    float out = sample * drive;
    for (int i = 0; i < folds; ++i)
    {
        if (out > 1.0f)
        {
            out = 2.0f - out;
        }
        else if (out < -1.0f)
        {
            out = -2.0f - out;
        }
    }
    return out;
}

inline float ApplyOverdrive(float sample, float amount)
{
    if (amount <= 0.0f)
    {
        return sample;
    }

    const float drive = 1.0f + amount * 4.0f;
    const float soft = std::tanh(sample * drive);
    if (amount < 0.5f)
    {
        return sample + (soft - sample) * (amount * 2.0f);
    }

    const float hard = std::clamp(sample, -1.0f, 1.0f);
    return soft + (hard - soft) * ((amount - 0.5f) * 2.0f);
}

struct DistortionChannel
{
    float makeupGain = 1.0f;

    void Reset() { makeupGain = 1.0f; }

    float ProcessSample(float input, const DistortionSettings &settings, float &inPeak, float &outPeak)
    {
        inPeak = std::max(inPeak, std::fabs(input));
        const float folded = ApplyWavefolder(input, settings.depth, settings.folds);
        const float driven = ApplyOverdrive(folded, settings.overdrive);
        outPeak = std::max(outPeak, std::fabs(driven));
        return driven * makeupGain;
    }

    void UpdateMakeup(float inPeak, float outPeak)
    {
        if (inPeak < 0.0005f || outPeak < 0.0005f)
        {
            makeupGain += (1.0f - makeupGain) * 0.02f;
            return;
        }

        float target = inPeak / outPeak;
        target = std::clamp(target, 0.25f, 4.0f);
        makeupGain += (target - makeupGain) * 0.05f;
    }
};
