#include "display.h"

#include <cstdio>

using namespace kxmx;

void RenderDisplay(Bluemchen &hw, const DisplayData &data)
{
    hw.display.Fill(false);

    char buf[32];

    hw.display.SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "Slm %s%c", data.processLabel, data.heartbeatOn ? '.' : ' ');
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
    default:
        snprintf(buf, sizeof(buf), "V%3d", vibe);
        break;
    }
    hw.display.WriteString(buf, Font_6x8, true);

    hw.display.Update();
}
