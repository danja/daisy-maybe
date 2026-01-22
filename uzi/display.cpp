#include "display.h"

#include <cstdio>

using namespace kxmx;

void RenderDisplay(Bluemchen &hw, const DisplayData &data)
{
    hw.display.Fill(false);

    char buf[32];

    hw.display.SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "%c%s%c",
             data.titleSelected ? '*' : ' ',
             data.pageTitle,
             data.heartbeatOn ? '.' : ' ');
    hw.display.WriteString(buf, Font_6x8, true);

    if (data.debug)
    {
        if (data.debugPage == 0)
        {
            hw.display.SetCursor(0, 8);
            snprintf(buf, sizeof(buf), "K1%04X", data.rawK1);
            hw.display.WriteString(buf, Font_6x8, true);

            hw.display.SetCursor(0, 16);
            snprintf(buf, sizeof(buf), "K2%04X", data.rawK2);
            hw.display.WriteString(buf, Font_6x8, true);

            hw.display.SetCursor(0, 24);
            snprintf(buf, sizeof(buf), "C1%04X", data.rawCv1);
            hw.display.WriteString(buf, Font_6x8, true);
        }
        else
        {
            hw.display.SetCursor(0, 8);
            snprintf(buf, sizeof(buf), "C2%04X", data.rawCv2);
            hw.display.WriteString(buf, Font_6x8, true);

            hw.display.SetCursor(0, 16);
            snprintf(buf, sizeof(buf), "ND%4d", static_cast<int>(data.notchDistance * 100.0f + 0.5f));
            hw.display.WriteString(buf, Font_6x8, true);

            hw.display.SetCursor(0, 24);
            snprintf(buf, sizeof(buf), "PH%4d", static_cast<int>(data.phaseOffset * 100.0f + 0.5f));
            hw.display.WriteString(buf, Font_6x8, true);
        }
    }
    else
    {
        for (int i = 0; i < data.lineCount; ++i)
        {
            const MenuLine &line = data.lines[i];
            const int row = 8 + i * 8;
            switch (line.type)
            {
            case MenuItemType::Percent:
                snprintf(buf, sizeof(buf), "%c%-4s %3d",
                         line.selected ? '*' : ' ',
                         line.label,
                         static_cast<int>(line.value * 100.0f + 0.5f));
                break;
            case MenuItemType::Ratio:
            {
                const int ratioCents = static_cast<int>(line.value * 100.0f + 0.5f);
                snprintf(buf, sizeof(buf), "%c%-4s %d.%02d",
                         line.selected ? '*' : ' ',
                         line.label,
                         ratioCents / 100,
                         ratioCents % 100);
                break;
            }
            case MenuItemType::Int:
                snprintf(buf, sizeof(buf), "%c%-4s %2d",
                         line.selected ? '*' : ' ',
                         line.label,
                         line.intValue);
                break;
            }
            hw.display.SetCursor(0, row);
            hw.display.WriteString(buf, Font_6x8, true);
        }
    }

    hw.display.Update();
}
