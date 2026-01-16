#pragma once

#include "kxmx_bluemchen.h"

struct DisplayData
{
    bool        isCalib = false;
    float       pitchScale = 1.0f;
    float       pitchOffset = 0.0f;
    float       currentFreq = 0.0f;
    float       currentFreq2 = 0.0f;
    float       feedback = 0.0f;
    float       damp = 0.0f;
    float       cross12 = 0.0f;
    float       cross21 = 0.0f;
    float       inputPos = 0.0f;
    float       reedAmount = 0.0f;
    float       reedBias = 0.5f;
    float       mix = 1.0f;
    bool        showSaveConfirm = false;
    const char *menuLabel = "FB";
    bool        encoderLongPress = false;
    bool        heartbeatOn = false;
    int         menuIndex = 0;
    int         encoderInc = 0;
    bool        encoderPressed = false;
    float       encoderHeldMs = 0.0f;
    int         encoderSteps = 0;
};

void RenderDisplay(kxmx::Bluemchen &hw, const DisplayData &data);
