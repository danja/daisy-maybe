#pragma once

#include <algorithm>
#include <cmath>

#include "daisysp.h"

struct FeedFilters
{
    daisysp::OnePole x;
    daisysp::OnePole x2;
    daisysp::OnePole y;
    daisysp::OnePole y2;

    void Init()
    {
        x.Init();
        x2.Init();
        y.Init();
        y2.Init();
    }

    void SetDampX(float damp)
    {
        const float cutoff = MapCutoff(damp);
        x.SetFrequency(cutoff);
        x2.SetFrequency(cutoff);
    }
    void SetDampY(float damp)
    {
        const float cutoff = MapCutoff(damp);
        y.SetFrequency(cutoff);
        y2.SetFrequency(cutoff);
    }

    float ProcessX(float v) { return x2.Process(x.Process(v)); }
    float ProcessY(float v) { return y2.Process(y.Process(v)); }

  private:
    static float MapCutoff(float damp)
    {
        const float minCut = 20.0f;
        const float maxCut = 12000.0f;
        const float clamped = std::clamp(damp, 0.0f, 1.0f);
        const float shaped = clamped * clamped;
        const float logMin = std::log(minCut);
        const float logMax = std::log(maxCut);
        const float t = 1.0f - shaped;
        return std::exp(logMin + t * (logMax - logMin));
    }
};

inline float SoftClipSample(float x)
{
    const float absx = fabsf(x);
    return x / (1.0f + absx);
}
