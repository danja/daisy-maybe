#pragma once

#include <cstdint>

#include "kxmx_bluemchen.h"

struct DisplayData
{
    const char *processLabel = "SMR";
    int         processIndex = 0;
    float       time1 = 1.0f;
    float       time2 = 1.0f;
    float       vibe = 0.0f;
    float       mix = 1.0f;
    float       cv1 = 0.0f;
    float       cv2 = 0.0f;
    uint16_t    rawK1 = 0;
    uint16_t    rawK2 = 0;
    uint16_t    rawCv1 = 0;
    uint16_t    rawCv2 = 0;
    int         menuPage = 0;
    bool        heartbeatOn = false;
};

void RenderDisplay(kxmx::Bluemchen &hw, const DisplayData &data);
