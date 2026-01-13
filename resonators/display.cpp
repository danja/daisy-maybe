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
        snprintf(buf, sizeof(buf), "Sc%.3f", data.pitchScale);
        hw.display.SetCursor(0, 8);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Of%+.2f", data.pitchOffset);
        hw.display.SetCursor(0, 16);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Pg%d I%d", data.menuIndex, data.encoderInc);
        hw.display.SetCursor(0, 24);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    else
    {
        snprintf(buf, sizeof(buf), "F1%.0f", data.currentFreq);
        hw.display.SetCursor(0, 8);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "F2%.0f", data.currentFreq2);
        hw.display.SetCursor(0, 16);
        hw.display.WriteString(buf, Font_6x8, true);

        switch (data.menuLabel[0])
        {
        case 'F': // FB
            snprintf(buf, sizeof(buf), "FB%.2f", data.feedback);
            break;
        case 'X': // X12/X21
            if (data.menuLabel[1] == '1')
                snprintf(buf, sizeof(buf), "X12%.2f", data.cross12);
            else
                snprintf(buf, sizeof(buf), "X21%.2f", data.cross21);
            break;
        case 'I': // INP
            snprintf(buf, sizeof(buf), "IN%.2f", data.inputPos);
            break;
        case 'D': // DAMP
            snprintf(buf, sizeof(buf), "DP%.2f", data.damp);
            break;
        case 'M': // MIX
            snprintf(buf, sizeof(buf), "MX%.2f", data.mix);
            break;
        default:
            snprintf(buf, sizeof(buf), "FB%.2f", data.feedback);
            break;
        }
        hw.display.SetCursor(0, 24);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    hw.display.Update();
}
