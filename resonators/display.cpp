#include "display.h"

#include <cstdio>

using namespace kxmx;

namespace
{
    constexpr float kCalibTone = 440.0f;
}

void RenderDisplay(Bluemchen &hw, const DisplayData &data)
{
    hw.display.Fill(false);

    char buf[32];

    hw.display.SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "Res %s%c", data.menuLabel, data.heartbeatOn ? '.' : ' ');
    hw.display.WriteString(buf, Font_6x8, true);

    if (data.isCalib)
    {
        const int scaleMilli = static_cast<int>(data.pitchScale * 1000.0f + 0.5f);
        const int offsetCents = static_cast<int>(data.pitchOffset * 100.0f + (data.pitchOffset >= 0.0f ? 0.5f : -0.5f));
        snprintf(buf, sizeof(buf), "Sc%4d", scaleMilli);
        hw.display.SetCursor(0, 8);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Of%+4d", offsetCents);
        hw.display.SetCursor(0, 16);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Pg%d I%d", data.menuIndex, data.encoderSteps);
        hw.display.SetCursor(0, 24);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    else
    {
        const int f1 = static_cast<int>(data.currentFreq + 0.5f);
        const int f2 = static_cast<int>(data.currentFreq2 + 0.5f);
        snprintf(buf, sizeof(buf), "F1%4d", f1);
        hw.display.SetCursor(0, 8);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "F2%4d", f2);
        hw.display.SetCursor(0, 16);
        hw.display.WriteString(buf, Font_6x8, true);

        switch (data.menuLabel[0])
        {
        case 'F': // FB
            snprintf(buf, sizeof(buf), "FB%3d", static_cast<int>(data.feedback * 100.0f + 0.5f));
            break;
        case 'X': // X12/X21
            if (data.menuLabel[1] == '1')
                snprintf(buf, sizeof(buf), "X12%2d", static_cast<int>(data.cross12 * 100.0f + 0.5f));
            else
                snprintf(buf, sizeof(buf), "X21%2d", static_cast<int>(data.cross21 * 100.0f + 0.5f));
            break;
        case 'I': // INP
            snprintf(buf, sizeof(buf), "IN%3d", static_cast<int>(data.inputPos * 100.0f + 0.5f));
            break;
        case 'D': // DAMP
            snprintf(buf, sizeof(buf), "DP%3d", static_cast<int>(data.damp * 100.0f + 0.5f));
            break;
        case 'M': // MIX
            snprintf(buf, sizeof(buf), "MX%3d", static_cast<int>(data.mix * 100.0f + 0.5f));
            break;
        default:
            snprintf(buf, sizeof(buf), "FB%3d", static_cast<int>(data.feedback * 100.0f + 0.5f));
            break;
        }
        hw.display.SetCursor(0, 24);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    hw.display.Update();
}
