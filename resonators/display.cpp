#include "display.h"

#include <cstdio>

using namespace kxmx;

namespace
{
/* Rounds away from zero. A cast truncates toward it, which reads a negative
 * value a whole unit short — -0.50 rendered as -49. */
int RoundHundredths(float v)
{
    return static_cast<int>(v * 100.0f + (v < 0.0f ? -0.5f : 0.5f));
}
} // namespace

void RenderDisplay(Bluemchen &hw, const DisplayData &data)
{
    hw.display.Fill(false);

    char buf[32];

    hw.display.SetCursor(0, 0);
    if (data.isCalib)
    {
        snprintf(buf, sizeof(buf), "Res CAL%c", data.heartbeatOn ? '.' : ' ');
    }
    else
    {
        snprintf(buf, sizeof(buf), "%c%s%c",
                 data.titleSelected ? '*' : ' ',
                 data.pageTitle,
                 data.heartbeatOn ? '.' : ' ');
    }
    hw.display.WriteString(buf, Font_6x8, true);

    if (data.showSaveConfirm)
    {
        hw.display.SetCursor(0, 8);
        hw.display.WriteString("CAL SAVED", Font_6x8, true);
        hw.display.SetCursor(0, 16);
        hw.display.WriteString("Hold to save", Font_6x8, true);
        hw.display.SetCursor(0, 24);
        hw.display.WriteString("Knobs active", Font_6x8, true);
    }
    else if (data.isCalib)
    {
        const int scaleMilli = static_cast<int>(data.pitchScale * 1000.0f + 0.5f);
        const int offsetCents = static_cast<int>(data.pitchOffset * 100.0f + (data.pitchOffset >= 0.0f ? 0.5f : -0.5f));
        snprintf(buf, sizeof(buf), "Sc%4d", scaleMilli);
        hw.display.SetCursor(0, 8);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Of%+4d", offsetCents);
        hw.display.SetCursor(0, 16);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Hz%4d", static_cast<int>(data.currentFreq + 0.5f));
        hw.display.SetCursor(0, 24);
        hw.display.WriteString(buf, Font_6x8, true);
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
                         RoundHundredths(line.value));
                break;
            case MenuItemType::Ratio:
            {
                /* Sign carried separately. Feedback runs -1.00 to +1.00, and
                 * splitting a negative into integer and remainder parts gives
                 * both of them a minus sign — "0.-50" for -0.5. */
                const int cents = RoundHundredths(line.value);
                const int mag = cents < 0 ? -cents : cents;
                snprintf(buf, sizeof(buf), "%c%-4s %s%d.%02d",
                         line.selected ? '*' : ' ',
                         line.label,
                         cents < 0 ? "-" : "",
                         mag / 100,
                         mag % 100);
                break;
            }
            case MenuItemType::Int:
                snprintf(buf, sizeof(buf), "%c%-4s %2d",
                         line.selected ? '*' : ' ',
                         line.label,
                         line.intValue);
                break;
            case MenuItemType::Enum:
            case MenuItemType::Action:
                /* 10 characters fit across the panel; WriteString stops at the
                 * right edge, so a long name clips rather than wrapping. */
                snprintf(buf, sizeof(buf), "%c%-4s %s",
                         line.selected ? '*' : ' ',
                         line.label,
                         line.text);
                break;
            case MenuItemType::Name:
                /* No label: the name is the row. */
                snprintf(buf, sizeof(buf), "%c%s",
                         line.selected ? '*' : ' ',
                         line.text);
                break;
            }
            hw.display.SetCursor(0, row);
            hw.display.WriteString(buf, Font_6x8, true);
        }
    }
    hw.display.Update();
}
