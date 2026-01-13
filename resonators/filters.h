#pragma once

#include <cmath>

#include "daisysp.h"

struct FeedbackFilters
{
    daisysp::OnePole fb1;
    daisysp::OnePole fb2;

    void Init()
    {
        fb1.Init();
        fb2.Init();
    }

    void SetDamp(float damp)
    {
        const float minCut = 50.0f;
        const float maxCut = 2500.0f;
        const float cutoff = minCut + (1.0f - damp) * (maxCut - minCut);
        fb1.SetFrequency(cutoff);
        fb2.SetFrequency(cutoff);
    }

    float Process1(float v) { return fb1.Process(v); }
    float Process2(float v) { return fb2.Process(v); }
};

inline float SoftClipSample(float x)
{
    const float absx = fabsf(x);
    return x / (1.0f + absx);
}
