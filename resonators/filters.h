#pragma once

#include <algorithm>
#include <cmath>

#include "daisysp.h"

struct FeedFilters
{
    daisysp::Svf x;
    daisysp::Svf y;

    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        x.Init(sampleRate);
        y.Init(sampleRate);
    }

    void SetParams(float level, float ratio, float q, float baseFreqX, float baseFreqY)
    {
        level_ = std::clamp(level, 0.0f, 1.0f);
        ratio_ = std::clamp(ratio, 0.25f, 2.0f);
        res_ = MapQToRes(q);

        x.SetFreq(ClampCutoff(baseFreqX * ratio_));
        y.SetFreq(ClampCutoff(baseFreqY * ratio_));
        x.SetRes(res_);
        y.SetRes(res_);
    }

    float ProcessX(float v)
    {
        x.Process(v);
        return Mix(v, x.Low(), level_);
    }
    float ProcessY(float v)
    {
        y.Process(v);
        return Mix(v, y.Low(), level_);
    }

  private:
    float sampleRate_ = 48000.0f;
    float level_ = 1.0f;
    float ratio_ = 1.0f;
    float res_ = 0.0f;

    float ClampCutoff(float freq) const
    {
        const float minCut = 20.0f;
        const float maxCut = 12000.0f;
        const float nyquistSafe = sampleRate_ / 3.0f;
        return std::clamp(freq, minCut, std::min(maxCut, nyquistSafe));
    }

    static float MapQToRes(float q)
    {
        const float clamped = std::clamp(q, 0.5f, 2.0f);
        return (clamped - 0.5f) / 1.5f;
    }

    static float Mix(float dry, float wet, float mix)
    {
        return dry + (wet - dry) * mix;
    }
};

inline float SoftClipSample(float x)
{
    const float absx = fabsf(x);
    return x / (1.0f + absx);
}
