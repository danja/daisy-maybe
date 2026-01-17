#pragma once

#include "kxmx_bluemchen.h"

struct DisplayData
{
    const char *processLabel = "Smear";
    float       time1 = 1.0f;
    float       time2 = 1.0f;
    float       vibe = 0.0f;
    float       mix = 1.0f;
    int         menuPage = 0;
    bool        heartbeatOn = false;
    bool        bypass = false;
    float       peak1 = 0.0f;
    float       peak2 = 0.0f;
    float       peakIn = 0.0f;
    float       peakOut = 0.0f;
    float       peakInClip = 0.0f;
    float       peakWet = 0.0f;
    float       cpuPercent = 0.0f;
    float       cpuMs = 0.0f;
    float       cpuBudgetMs = 0.0f;
    float       preserve = 0.2f;
    float       spectralGain = 1.0f;
    float       ifftGain = 1.0f;
    float       olaGain = 1.0f;
    const char *windowLabel = "SQH";
    float       kaiserBeta = 6.0f;
    bool        phaseContinuity = true;
    int         wetClampMode = 1;
    bool        normalizeSpectrum = true;
    bool        limitSpectrum = true;
    uint16_t    rawK1 = 0;
    uint16_t    rawK2 = 0;
    uint16_t    rawCv1 = 0;
    uint16_t    rawCv2 = 0;
};

void RenderDisplay(kxmx::Bluemchen &hw, const DisplayData &data);
