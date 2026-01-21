#pragma once

#include "kxmx_bluemchen.h"
#include "menu_system.h"

struct DisplayData
{
    bool isCalib = false;
    bool showSaveConfirm = false;
    float pitchScale = 1.0f;
    float pitchOffset = 0.0f;
    float currentFreq = 0.0f;
    bool heartbeatOn = false;

    const char *pageTitle = "";
    bool titleSelected = false;
    MenuLine lines[3]{};
    int lineCount = 0;
};

void RenderDisplay(kxmx::Bluemchen &hw, const DisplayData &data);
