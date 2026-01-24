#pragma once

struct UziState
{
    float mix = 1.0f;
    float feedback = 0.0f;
    float xmix = 0.0f;
    float lfoDepth = 0.0f;
    float lfoFreq = 0.2f;
    float cutoffHz = 100.0f;

    float wave = 0.0f;
    float overdrive = 0.0f;

    float crossover = 0.0f;
    float blur = 0.5f;
    float binRounding = 0.0f;
    int blockSize = 0;

    float notchDistance = 0.5f;
    float phaseOffset = 0.0f;
};

struct UziRuntime
{
    float mix = 1.0f;
    float feedback = 0.0f;
    float xmix = 0.0f;
    float lfoDepth = 0.0f;
    float lfoFreq = 0.2f;
    float cutoffHz = 100.0f;

    float wave = 0.0f;
    float overdrive = 0.0f;

    float crossover = 0.0f;
    float blur = 0.5f;
    float binRounding = 0.0f;
    int blockSize = 0;

    float notchDistance = 0.5f;
    float phaseOffset = 0.0f;

    uint16_t rawK1 = 0;
    uint16_t rawK2 = 0;
    uint16_t rawCv1 = 0;
    uint16_t rawCv2 = 0;
};
