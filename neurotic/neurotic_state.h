#pragma once

#include <cstdint>

struct NeuroticState
{
    float mix = 0.8f;
    float fb = 0.0f;
    float outTrim = 1.0f;
    int algoIndex = 0;
    float lfoDepth = 0.0f;
    float lfoRate = 0.2f;
    float c3 = 0.0f;
    float c4 = 0.5f;
};

struct NeuroticRuntime
{
    float mix = 0.8f;
    float fb = 0.0f;
    float outTrim = 1.0f;
    int algoIndex = 0;
    float c1 = 0.0f;
    float c2 = 0.0f;
    float c3 = 0.5f;
    float c4 = 0.5f;
    float lfoDepth = 0.0f;
    float lfoRate = 0.2f;
    float lfoValue = 0.0f;

    uint16_t rawK1 = 0;
    uint16_t rawK2 = 0;
    uint16_t rawCv1 = 0;
    uint16_t rawCv2 = 0;
};
