#pragma once

#include "kxmx_bluemchen.h"
#include "menu_system.h"

struct DisplayData
{
    const char *pageTitle = "";
    bool titleSelected = false;
    MenuLine lines[3]{};
    int lineCount = 0;
    bool heartbeatOn = false;
    bool debug = false;
    int debugPage = 0;
    uint16_t rawK1 = 0;
    uint16_t rawK2 = 0;
    uint16_t rawCv1 = 0;
    uint16_t rawCv2 = 0;
    float notchDistance = 0.0f;
    float phaseOffset = 0.0f;
};

void RenderDisplay(kxmx::Bluemchen &hw, const DisplayData &data);
