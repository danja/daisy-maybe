#pragma once

#include "kxmx_bluemchen.h"

struct DisplayData
{
    const char *processLabel = "SMR";
    float       time1 = 1.0f;
    float       time2 = 1.0f;
    float       vibe = 0.0f;
    float       mix = 1.0f;
    int         menuPage = 0;
    bool        heartbeatOn = false;
};

void RenderDisplay(kxmx::Bluemchen &hw, const DisplayData &data);
