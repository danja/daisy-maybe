#include "display.h"

#include <algorithm>
#include <cstdio>

using namespace kxmx;

void RenderDisplay(Bluemchen &hw, const DisplayData &data)
{
    hw.display.Fill(false);

    char buf[32];

    hw.display.SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "%s%c", data.processLabel, data.heartbeatOn ? '.' : ' ');
    hw.display.WriteString(buf, Font_6x8, true);

    const int t1 = static_cast<int>(data.time1 * 100.0f + 0.5f);
    const int t2 = static_cast<int>(data.time2 * 100.0f + 0.5f);
    const int vibe = static_cast<int>(data.vibe * 100.0f + 0.5f);
    const int mix = static_cast<int>(data.mix * 100.0f + 0.5f);

    hw.display.SetCursor(0, 8);
    snprintf(buf, sizeof(buf), "T1%4d", t1);
    hw.display.WriteString(buf, Font_6x8, true);

    hw.display.SetCursor(0, 16);
    snprintf(buf, sizeof(buf), "T2%4d", t2);
    hw.display.WriteString(buf, Font_6x8, true);

    hw.display.SetCursor(0, 24);
    switch (data.menuPage)
    {
    case 0:
        snprintf(buf, sizeof(buf), "V%3d", vibe);
        break;
    case 1:
        snprintf(buf, sizeof(buf), "R%3d", static_cast<int>((data.time2 / data.time1) * 100.0f + 0.5f));
        break;
    case 2:
        snprintf(buf, sizeof(buf), "MX%3d", mix);
        break;
    case 3:
        snprintf(buf, sizeof(buf), "BYP %s", data.bypass ? "ON" : "OFF");
        break;
    case 4:
        snprintf(buf, sizeof(buf), "PR%3d", static_cast<int>(data.preserve * 100.0f + 0.5f));
        break;
    case 5:
        snprintf(buf, sizeof(buf), "SP%3d", static_cast<int>(data.spectralGain * 100.0f + 0.5f));
        break;
    case 6:
        snprintf(buf, sizeof(buf), "IF%3d", static_cast<int>(data.ifftGain * 100.0f + 0.5f));
        break;
    case 7:
        snprintf(buf, sizeof(buf), "OL%3d", static_cast<int>(data.olaGain * 100.0f + 0.5f));
        break;
    case 8:
        snprintf(buf, sizeof(buf), "%s", data.windowLabel);
        break;
    case 9:
        snprintf(buf, sizeof(buf), "KB%3d", static_cast<int>(data.kaiserBeta + 0.5f));
        break;
    case 10:
        snprintf(buf, sizeof(buf), "PH %s", data.phaseContinuity ? "ON" : "OFF");
        break;
    case 11:
        snprintf(buf, sizeof(buf), "WCL%d", data.wetClampMode);
        break;
    case 12:
        snprintf(buf, sizeof(buf), "NRM %s", data.normalizeSpectrum ? "ON" : "OFF");
        break;
    case 13:
        snprintf(buf, sizeof(buf), "LIM %s", data.limitSpectrum ? "ON" : "OFF");
        break;
    case 14:
        snprintf(buf, sizeof(buf), "IN");
        break;
    case 15:
        snprintf(buf, sizeof(buf), "WET");
        break;
    case 16:
        snprintf(buf, sizeof(buf), "CPU");
        break;
    case 17:
        snprintf(buf, sizeof(buf), "KNB");
        break;
    default:
        snprintf(buf, sizeof(buf), "V%3d", vibe);
        break;
    }
    hw.display.WriteString(buf, Font_6x8, true);

    if (data.menuPage == 14)
    {
        const int pin = static_cast<int>(data.peakIn * 1000.0f + 0.5f);
        const int pclip = static_cast<int>(data.peakInClip * 1000.0f + 0.5f);
        const int pout = static_cast<int>(data.peakOut * 1000.0f + 0.5f);
        hw.display.SetCursor(0, 8);
        snprintf(buf, sizeof(buf), "IN%4d", pin);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 16);
        snprintf(buf, sizeof(buf), "CL%4d", pclip);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 24);
        snprintf(buf, sizeof(buf), "OT%4d", pout);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    else if (data.menuPage == 15)
    {
        const int pwet = static_cast<int>(data.peakWet * 1000.0f + 0.5f);
        const int p1 = static_cast<int>(data.peak1 * 1000.0f + 0.5f);
        const int p2 = static_cast<int>(data.peak2 * 1000.0f + 0.5f);
        hw.display.SetCursor(0, 8);
        snprintf(buf, sizeof(buf), "WT%4d", pwet);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 16);
        snprintf(buf, sizeof(buf), "M1%4d", p1);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 24);
        snprintf(buf, sizeof(buf), "M2%4d", p2);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    else if (data.menuPage == 16)
    {
        const int load = static_cast<int>(data.cpuPercent + 0.5f);
        const int ms = static_cast<int>(data.cpuMs * 10.0f + 0.5f);
        const int budget = static_cast<int>(data.cpuBudgetMs * 10.0f + 0.5f);
        hw.display.SetCursor(0, 8);
        snprintf(buf, sizeof(buf), "LD%3d", load);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 16);
        snprintf(buf, sizeof(buf), "MS%3d", ms);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 24);
        snprintf(buf, sizeof(buf), "BD%3d", budget);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    else if (data.menuPage == 17)
    {
        hw.display.SetCursor(0, 8);
        snprintf(buf, sizeof(buf), "K1%04X", data.rawK1);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 16);
        snprintf(buf, sizeof(buf), "K2%04X", data.rawK2);
        hw.display.WriteString(buf, Font_6x8, true);

        hw.display.SetCursor(0, 24);
        snprintf(buf, sizeof(buf), "C%04X", data.rawCv2);
        hw.display.WriteString(buf, Font_6x8, true);
    }

    hw.display.Update();
}
