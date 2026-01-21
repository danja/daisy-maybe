#pragma once

#include <algorithm>
#include <cmath>

#include "daisysp.h"

struct FeedFilters
{
    daisysp::OnePole x;
    daisysp::OnePole y;

    void Init()
    {
        x.Init();
        y.Init();
    }

    void SetDampX(float damp) { x.SetFrequency(MapCutoff(damp)); }
    void SetDampY(float damp) { y.SetFrequency(MapCutoff(damp)); }

    float ProcessX(float v) { return x.Process(v); }
    float ProcessY(float v) { return y.Process(v); }

  private:
    static float MapCutoff(float damp)
    {
        const float minCut = 50.0f;
        const float maxCut = 4000.0f;
        const float clamped = std::clamp(damp, 0.0f, 1.0f);
        return minCut + (1.0f - clamped) * (maxCut - minCut);
    }
};

inline float SoftClipSample(float x)
{
    const float absx = fabsf(x);
    return x / (1.0f + absx);
}
